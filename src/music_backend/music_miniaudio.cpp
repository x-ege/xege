#if defined(__linux__)

#include "music_backend.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
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

DWORD millisecondsFromFrames(ma_uint64 frames, ma_uint32 sampleRate)
{
    if (sampleRate == 0) {
        return MUSIC_ERROR;
    }

    const ma_uint64 milliseconds = frames * 1000 / sampleRate;
    return static_cast<DWORD>(
        std::min<ma_uint64>(milliseconds, MUSIC_ERROR - 1));
}

ma_uint64 framesFromMilliseconds(DWORD milliseconds, ma_uint32 sampleRate)
{
    return static_cast<ma_uint64>(milliseconds) * sampleRate / 1000;
}

bool useNullAudioBackend()
{
    const char* backend = std::getenv("EGE_MUSIC_AUDIO_BACKEND");
    return backend != nullptr && std::string(backend) == "null";
}

class MiniaudioMusicBackend final : public MusicBackend
{
public:
    MiniaudioMusicBackend() = default;

    ~MiniaudioMusicBackend() override
    {
        Close();
    }

    DWORD Open(const std::string& path) override
    {
        if (path.empty()) {
            return MUSIC_ERROR;
        }

        ma_result result;
        if (useNullAudioBackend()) {
            const ma_backend backends[] = {ma_backend_null};
            result = ma_context_init(backends, 1, nullptr, &m_context);
        } else {
            result = ma_context_init(nullptr, 0, nullptr, &m_context);
        }
        if (result != MA_SUCCESS) {
            return MUSIC_ERROR;
        }
        m_contextInitialized = true;

        ma_engine_config engineConfig = ma_engine_config_init();
        engineConfig.pContext         = &m_context;
        result                        = ma_engine_init(&engineConfig, &m_engine);
        if (result != MA_SUCCESS) {
            releaseResources();
            return MUSIC_ERROR;
        }
        m_engineInitialized = true;

        result = ma_sound_init_from_file(
            &m_engine, path.c_str(),
            MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr,
            nullptr, &m_sound);
        if (result != MA_SUCCESS) {
            releaseResources();
            return MUSIC_ERROR;
        }
        m_soundInitialized = true;

        ma_uint64 lengthFrames = 0;
        ma_uint32 sampleRate   = 0;
        if (ma_sound_get_data_format(
                &m_sound, nullptr, nullptr, &sampleRate, nullptr, 0) !=
                MA_SUCCESS ||
            sampleRate == 0 ||
            ma_sound_get_length_in_pcm_frames(&m_sound, &lengthFrames) !=
                MA_SUCCESS) {
            releaseResources();
            return MUSIC_ERROR;
        }

        m_sampleRate   = sampleRate;
        m_lengthFrames = lengthFrames;
        m_status       = MUSIC_MODE_STOP;
        m_open         = true;
        m_exit         = false;

        try {
            m_controlThread =
                std::thread(&MiniaudioMusicBackend::controlLoop, this);
        } catch (...) {
            m_open = false;
            releaseResources();
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

        ma_uint64 cursor = 0;
        if (ma_sound_get_cursor_in_pcm_frames(&m_sound, &cursor) !=
            MA_SUCCESS) {
            return MUSIC_ERROR;
        }

        const bool defaultFullLoop =
            repeat && from == MUSIC_ERROR && to == MUSIC_ERROR;
        ma_uint64 startFrame =
            from == MUSIC_ERROR
                ? (defaultFullLoop ? 0 : cursor)
                : framesFromMilliseconds(from, m_sampleRate);
        ma_uint64 endFrame =
            to == MUSIC_ERROR
                ? m_lengthFrames
                : framesFromMilliseconds(to, m_sampleRate);

        startFrame = std::min(startFrame, m_lengthFrames);
        endFrame   = std::min(endFrame, m_lengthFrames);
        if (startFrame >= endFrame) {
            return MUSIC_ERROR;
        }

        if (from != MUSIC_ERROR || defaultFullLoop ||
            ma_sound_at_end(&m_sound)) {
            if (ma_sound_seek_to_pcm_frame(&m_sound, startFrame) !=
                MA_SUCCESS) {
                return MUSIC_ERROR;
            }
        }

        m_rangeStart = startFrame;
        m_rangeEnd   = endFrame;
        m_repeat     = repeat;
        m_hasRange = to != MUSIC_ERROR ||
                     (repeat && !defaultFullLoop && startFrame != 0);

        const bool nativeFullLoop =
            repeat && startFrame == 0 && endFrame == m_lengthFrames &&
            !m_hasRange;
        ma_sound_set_looping(&m_sound,
                             nativeFullLoop ? MA_TRUE : MA_FALSE);

        if (ma_sound_start(&m_sound) != MA_SUCCESS) {
            return MUSIC_ERROR;
        }

        m_status = MUSIC_MODE_PLAY;
        m_condition.notify_all();
        return kSuccess;
    }

    DWORD Pause() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || ma_sound_stop(&m_sound) != MA_SUCCESS) {
            return MUSIC_ERROR;
        }
        m_status = MUSIC_MODE_PAUSE;
        return kSuccess;
    }

    DWORD Stop() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || ma_sound_stop(&m_sound) != MA_SUCCESS) {
            return MUSIC_ERROR;
        }
        ma_sound_set_looping(&m_sound, MA_FALSE);
        m_status = MUSIC_MODE_STOP;
        return kSuccess;
    }

    DWORD Seek(DWORD to) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        const ma_uint64 frame = framesFromMilliseconds(to, m_sampleRate);
        if (frame > m_lengthFrames ||
            ma_sound_seek_to_pcm_frame(&m_sound, frame) != MA_SUCCESS) {
            return MUSIC_ERROR;
        }
        return kSuccess;
    }

    DWORD SetVolume(float value) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || !std::isfinite(value)) {
            return MUSIC_ERROR;
        }
        ma_sound_set_volume(&m_sound, std::clamp(value, 0.0f, 1.0f));
        return kSuccess;
    }

    DWORD Close() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_open && !m_contextInitialized) {
                return kSuccess;
            }
            m_exit = true;
            if (m_soundInitialized) {
                ma_sound_stop(&m_sound);
            }
        }
        m_condition.notify_all();

        if (m_controlThread.joinable()) {
            m_controlThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_open   = false;
        m_status = MUSIC_MODE_NOT_OPEN;
        releaseResources();
        return kSuccess;
    }

    DWORD GetPosition() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        ma_uint64 cursor = 0;
        return ma_sound_get_cursor_in_pcm_frames(&m_sound, &cursor) ==
                       MA_SUCCESS
                   ? millisecondsFromFrames(cursor, m_sampleRate)
                   : MUSIC_ERROR;
    }

    DWORD GetLength() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open
                   ? millisecondsFromFrames(m_lengthFrames, m_sampleRate)
                   : MUSIC_ERROR;
    }

    DWORD GetPlayStatus() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_MODE_NOT_OPEN;
        }
        refreshEndStateLocked();
        return m_status;
    }

private:
    void controlLoop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_exit) {
            m_condition.wait_for(lock, std::chrono::milliseconds(5),
                                 [this] { return m_exit; });
            if (m_exit || !m_open || m_status != MUSIC_MODE_PLAY) {
                continue;
            }

            ma_uint64 cursor = 0;
            if (ma_sound_get_cursor_in_pcm_frames(&m_sound, &cursor) !=
                MA_SUCCESS) {
                m_status = MUSIC_MODE_NOT_READY;
                continue;
            }

            const bool reachedRangeEnd =
                m_hasRange && cursor >= m_rangeEnd;
            const bool reachedFileEnd = ma_sound_at_end(&m_sound) != MA_FALSE;
            if (!reachedRangeEnd && !reachedFileEnd) {
                continue;
            }

            if (m_repeat) {
                ma_sound_stop(&m_sound);
                if (ma_sound_seek_to_pcm_frame(&m_sound, m_rangeStart) !=
                        MA_SUCCESS ||
                    ma_sound_start(&m_sound) != MA_SUCCESS) {
                    m_status = MUSIC_MODE_NOT_READY;
                }
            } else {
                ma_sound_stop(&m_sound);
                if (reachedRangeEnd) {
                    ma_sound_seek_to_pcm_frame(&m_sound, m_rangeEnd);
                }
                m_status = MUSIC_MODE_STOP;
            }
        }
    }

    void refreshEndStateLocked()
    {
        if (m_status == MUSIC_MODE_PLAY &&
            ma_sound_at_end(&m_sound) != MA_FALSE && !m_repeat) {
            m_status = MUSIC_MODE_STOP;
        }
    }

    void releaseResources()
    {
        if (m_soundInitialized) {
            ma_sound_uninit(&m_sound);
            m_soundInitialized = false;
        }
        if (m_engineInitialized) {
            ma_engine_uninit(&m_engine);
            m_engineInitialized = false;
        }
        if (m_contextInitialized) {
            ma_context_uninit(&m_context);
            m_contextInitialized = false;
        }
    }

    ma_context m_context{};
    ma_engine  m_engine{};
    ma_sound   m_sound{};

    std::mutex              m_mutex;
    std::condition_variable m_condition;
    std::thread             m_controlThread;

    ma_uint32 m_sampleRate   = 0;
    ma_uint64 m_lengthFrames = 0;
    ma_uint64 m_rangeStart   = 0;
    ma_uint64 m_rangeEnd     = 0;
    DWORD     m_status       = MUSIC_MODE_NOT_OPEN;
    bool      m_contextInitialized = false;
    bool      m_engineInitialized  = false;
    bool      m_soundInitialized   = false;
    bool      m_open               = false;
    bool      m_exit               = false;
    bool      m_repeat             = false;
    bool      m_hasRange           = false;
};

} // namespace

std::unique_ptr<MusicBackend> CreateMiniaudioMusicBackend()
{
    return std::make_unique<MiniaudioMusicBackend>();
}

} // namespace detail
} // namespace ege

#endif
