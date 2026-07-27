#if defined(__linux__) && defined(EGE_MUSIC_HAS_GSTREAMER)

#include "ege.h"
#include "music_backend.h"

#include <gst/gst.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <thread>

namespace ege
{
namespace detail
{
namespace
{

constexpr DWORD kSuccess = 0;

bool initializeGStreamer()
{
    static std::once_flag flag;
    static bool           initialized = false;
    std::call_once(flag, [] {
        GError* error = nullptr;
        initialized  = gst_init_check(nullptr, nullptr, &error) != FALSE;
        if (error != nullptr) {
            g_error_free(error);
        }
    });
    return initialized;
}

bool useNullAudioBackend()
{
    const char* backend = std::getenv("EGE_MUSIC_AUDIO_BACKEND");
    return backend != nullptr && std::string(backend) == "null";
}

DWORD millisecondsFromClockTime(gint64 time)
{
    if (time < 0) {
        return MUSIC_ERROR;
    }
    return static_cast<DWORD>(
        std::min<guint64>(static_cast<guint64>(time / GST_MSECOND),
                          MUSIC_ERROR - 1));
}

class GStreamerMusicBackend final : public MusicBackend
{
public:
    GStreamerMusicBackend() = default;

    ~GStreamerMusicBackend() override
    {
        Close();
    }

    DWORD Open(const std::string& path) override
    {
        if (path.empty() || !initializeGStreamer()) {
            return MUSIC_ERROR;
        }

        GError* error     = nullptr;
        gchar*  absolute  = g_canonicalize_filename(path.c_str(), nullptr);
        gchar*  uri       = gst_filename_to_uri(absolute, &error);
        g_free(absolute);
        if (uri == nullptr) {
            if (error != nullptr) {
                g_error_free(error);
            }
            return MUSIC_ERROR;
        }

        m_pipeline = gst_element_factory_make("playbin", nullptr);
        if (m_pipeline == nullptr) {
            g_free(uri);
            return MUSIC_ERROR;
        }
        // Factory-created GstObjects have a floating reference. playbin is a
        // top-level pipeline (it is never added to a parent bin), so claim
        // that reference explicitly before asynchronous state changes begin.
        gst_object_ref_sink(m_pipeline);
        g_object_set(m_pipeline, "uri", uri, nullptr);
        g_free(uri);

        if (useNullAudioBackend()) {
            GstElement* sink = gst_element_factory_make("fakesink", nullptr);
            if (sink == nullptr) {
                releasePipeline();
                return MUSIC_ERROR;
            }
            gst_object_ref_sink(sink);
            g_object_set(sink, "sync", TRUE, nullptr);
            g_object_set(m_pipeline, "audio-sink", sink, nullptr);
            gst_object_unref(sink);
        }

        m_bus = gst_element_get_bus(m_pipeline);
        if (m_bus == nullptr ||
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED) ==
                GST_STATE_CHANGE_FAILURE) {
            releasePipeline();
            return MUSIC_ERROR;
        }

        const GstStateChangeReturn stateResult =
            gst_element_get_state(m_pipeline, nullptr, nullptr,
                                  10 * GST_SECOND);
        if (stateResult == GST_STATE_CHANGE_FAILURE ||
            hasPendingErrorMessage()) {
            releasePipeline();
            return MUSIC_ERROR;
        }

        gint64 duration = GST_CLOCK_TIME_NONE;
        if (!gst_element_query_duration(m_pipeline, GST_FORMAT_TIME,
                                        &duration) ||
            duration <= 0) {
            releasePipeline();
            return MUSIC_ERROR;
        }

        m_duration = duration;
        m_status   = MUSIC_MODE_STOP;
        m_open     = true;
        m_exit.store(false);

        try {
            m_busThread =
                std::thread(&GStreamerMusicBackend::busLoop, this);
        } catch (...) {
            m_open = false;
            releasePipeline();
            return MUSIC_ERROR;
        }
        return kSuccess;
    }

    DWORD Play(DWORD from, DWORD to, bool repeat) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        gint64 current = 0;
        if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME,
                                        &current)) {
            current = 0;
        }

        const bool defaultFullLoop =
            repeat && from == MUSIC_ERROR && to == MUSIC_ERROR;
        gint64 start =
            from == MUSIC_ERROR
                ? (defaultFullLoop ? 0 : current)
                : static_cast<gint64>(from) * GST_MSECOND;
        gint64 end = to == MUSIC_ERROR
                         ? m_duration
                         : static_cast<gint64>(to) * GST_MSECOND;
        start = std::clamp<gint64>(start, 0, m_duration);
        end   = std::clamp<gint64>(end, 0, m_duration);
        if (start >= end) {
            return MUSIC_ERROR;
        }

        const bool needsSeek =
            from != MUSIC_ERROR || to != MUSIC_ERROR || repeat ||
            current >= m_duration;
        if (needsSeek && !seekRangeLocked(start, end, repeat)) {
            return MUSIC_ERROR;
        }

        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) ==
            GST_STATE_CHANGE_FAILURE) {
            return MUSIC_ERROR;
        }

        m_rangeStart   = start;
        m_rangeEnd     = end;
        m_lastPosition = start;
        m_repeat       = repeat;
        m_status       = MUSIC_MODE_PLAY;
        return kSuccess;
    }

    DWORD Pause() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open ||
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED) ==
                GST_STATE_CHANGE_FAILURE) {
            return MUSIC_ERROR;
        }
        m_status = MUSIC_MODE_PAUSE;
        return kSuccess;
    }

    DWORD Stop() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open ||
            gst_element_set_state(m_pipeline, GST_STATE_PAUSED) ==
                GST_STATE_CHANGE_FAILURE) {
            return MUSIC_ERROR;
        }
        m_repeat = false;
        m_status = MUSIC_MODE_STOP;
        return kSuccess;
    }

    DWORD Seek(DWORD to) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        const gint64 target = static_cast<gint64>(to) * GST_MSECOND;
        if (target > m_duration ||
            !gst_element_seek_simple(
                m_pipeline, GST_FORMAT_TIME,
                static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                          GST_SEEK_FLAG_ACCURATE),
                target)) {
            return MUSIC_ERROR;
        }
        m_lastPosition = target;
        return kSuccess;
    }

    DWORD SetVolume(float value) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || !std::isfinite(value)) {
            return MUSIC_ERROR;
        }
        const gdouble volume =
            static_cast<gdouble>(std::clamp(value, 0.0f, 1.0f));
        g_object_set(m_pipeline, "volume", volume, nullptr);
        return kSuccess;
    }

    DWORD Close() override
    {
        m_exit.store(true);
        if (m_busThread.joinable()) {
            m_busThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open && m_pipeline == nullptr) {
            return kSuccess;
        }
        m_open   = false;
        m_status = MUSIC_MODE_NOT_OPEN;
        releasePipeline();
        return kSuccess;
    }

    DWORD GetPosition() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        gint64 position = GST_CLOCK_TIME_NONE;
        if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME,
                                        &position)) {
            return millisecondsFromClockTime(m_lastPosition);
        }

        // A flushing segment seek can briefly expose the previous timeline
        // (commonly zero) while the new segment is being prerolled. Keep the
        // public position inside the requested repeat range during that
        // transition.
        if (m_repeat) {
            position =
                std::clamp(position, m_rangeStart, m_rangeEnd);
        }
        m_lastPosition = position;
        return millisecondsFromClockTime(position);
    }

    DWORD GetLength() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open ? millisecondsFromClockTime(m_duration) : MUSIC_ERROR;
    }

    DWORD GetPlayStatus() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open ? m_status : MUSIC_MODE_NOT_OPEN;
    }

private:
    bool seekRangeLocked(gint64 start, gint64 end, bool segment)
    {
        GstSeekFlags flags =
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH |
                                      GST_SEEK_FLAG_ACCURATE);
        if (segment) {
            flags =
                static_cast<GstSeekFlags>(flags | GST_SEEK_FLAG_SEGMENT);
        }

        return gst_element_seek(
                   m_pipeline, 1.0, GST_FORMAT_TIME, flags,
                   GST_SEEK_TYPE_SET, start, GST_SEEK_TYPE_SET, end) !=
               FALSE;
    }

    bool hasPendingErrorMessage()
    {
        GstMessage* message =
            gst_bus_pop_filtered(m_bus, GST_MESSAGE_ERROR);
        if (message == nullptr) {
            return false;
        }
        gst_message_unref(message);
        return true;
    }

    void busLoop()
    {
        const GstMessageType watched =
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS |
                                        GST_MESSAGE_SEGMENT_DONE);
        while (!m_exit.load()) {
            GstMessage* message =
                gst_bus_timed_pop_filtered(m_bus, 10 * GST_MSECOND, watched);
            if (message == nullptr) {
                continue;
            }

            const GstMessageType type = GST_MESSAGE_TYPE(message);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_open) {
                    if (type == GST_MESSAGE_ERROR) {
                        m_status = MUSIC_MODE_NOT_READY;
                    } else if (type == GST_MESSAGE_EOS ||
                               type == GST_MESSAGE_SEGMENT_DONE) {
                        if (m_repeat &&
                            seekRangeLocked(m_rangeStart, m_rangeEnd, true) &&
                            gst_element_set_state(
                                m_pipeline, GST_STATE_PLAYING) !=
                                GST_STATE_CHANGE_FAILURE) {
                            m_lastPosition = m_rangeStart;
                            m_status = MUSIC_MODE_PLAY;
                        } else {
                            gst_element_set_state(m_pipeline,
                                                  GST_STATE_PAUSED);
                            m_status = MUSIC_MODE_STOP;
                        }
                    }
                }
            }
            gst_message_unref(message);
        }
    }

    void releasePipeline()
    {
        if (m_pipeline != nullptr) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
        }
        if (m_bus != nullptr) {
            gst_object_unref(m_bus);
            m_bus = nullptr;
        }
        if (m_pipeline != nullptr) {
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
        }
    }

    GstElement* m_pipeline = nullptr;
    GstBus*     m_bus      = nullptr;

    std::mutex       m_mutex;
    std::thread      m_busThread;
    std::atomic_bool m_exit{false};

    gint64 m_duration     = 0;
    gint64 m_rangeStart   = 0;
    gint64 m_rangeEnd     = 0;
    gint64 m_lastPosition = 0;
    DWORD  m_status       = MUSIC_MODE_NOT_OPEN;
    bool   m_open         = false;
    bool   m_repeat       = false;
};

} // namespace

std::unique_ptr<MusicBackend> CreateGStreamerMusicBackend()
{
    return std::make_unique<GStreamerMusicBackend>();
}

} // namespace detail
} // namespace ege

#endif
