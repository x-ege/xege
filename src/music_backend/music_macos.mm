#if defined(__APPLE__)

#include "music_backend.h"

#import <AVFAudio/AVFAudio.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace ege
{
namespace detail
{
namespace
{

constexpr DWORD kSuccess = 0;

DWORD millisecondsFromSeconds(NSTimeInterval seconds)
{
    if (!std::isfinite(seconds) || seconds < 0) {
        return MUSIC_ERROR;
    }
    const double milliseconds = seconds * 1000.0;
    return static_cast<DWORD>(
        std::min<double>(milliseconds, MUSIC_ERROR - 1));
}

class MacOSMusicBackend final : public MusicBackend
{
public:
    MacOSMusicBackend() = default;

    ~MacOSMusicBackend() override
    {
        Close();
    }

    DWORD Open(const std::string& path) override
    {
        if (path.empty()) {
            return MUSIC_ERROR;
        }

        @autoreleasepool {
            NSString* filePath =
                [[NSString alloc] initWithUTF8String:path.c_str()];
            if (filePath == nil) {
                return MUSIC_ERROR;
            }
            NSURL* fileURL = [NSURL fileURLWithPath:filePath];

            NSError* error = nil;
            m_audioPlayer =
                [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL
                                                      error:&error];
            if (m_audioPlayer != nil && [m_audioPlayer prepareToPlay]) {
                m_kind     = PlayerKind::Audio;
                m_duration = m_audioPlayer.duration;
            } else {
                m_audioPlayer = nil;
                if (!openMIDI(fileURL)) {
                    releasePlayersLocked();
                    return MUSIC_ERROR;
                }
            }

            if (!(m_duration > 0) || !std::isfinite(m_duration)) {
                releasePlayersLocked();
                return MUSIC_ERROR;
            }

            m_status = MUSIC_MODE_STOP;
            m_open   = true;
            m_exit   = false;
            try {
                m_controlThread =
                    std::thread(&MacOSMusicBackend::controlLoop, this);
            } catch (...) {
                m_open = false;
                releasePlayersLocked();
                return MUSIC_ERROR;
            }
        }
        return kSuccess;
    }

    DWORD Play(DWORD from, DWORD to, bool repeat) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        @autoreleasepool {
            const bool defaultFullLoop =
                repeat && from == MUSIC_ERROR && to == MUSIC_ERROR;
            NSTimeInterval start =
                from == MUSIC_ERROR
                    ? (defaultFullLoop ? 0 : currentPositionLocked())
                    : static_cast<NSTimeInterval>(from) / 1000.0;
            NSTimeInterval end =
                to == MUSIC_ERROR
                    ? m_duration
                    : static_cast<NSTimeInterval>(to) / 1000.0;
            start = std::clamp<NSTimeInterval>(start, 0, m_duration);
            end   = std::clamp<NSTimeInterval>(end, 0, m_duration);
            if (start >= end) {
                return MUSIC_ERROR;
            }

            if (from != MUSIC_ERROR || defaultFullLoop ||
                currentPositionLocked() >= m_duration) {
                setPositionLocked(start);
            }

            m_rangeStart = start;
            m_rangeEnd   = end;
            m_repeat     = repeat;
            if (!startLocked()) {
                return MUSIC_ERROR;
            }

            m_status = MUSIC_MODE_PLAY;
            m_condition.notify_all();
            return kSuccess;
        }
    }

    DWORD Pause() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }
        @autoreleasepool {
            stopLocked();
            m_status = MUSIC_MODE_PAUSE;
        }
        return kSuccess;
    }

    DWORD Stop() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }
        @autoreleasepool {
            stopLocked();
            m_repeat = false;
            m_status = MUSIC_MODE_STOP;
        }
        return kSuccess;
    }

    DWORD Seek(DWORD to) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }

        const NSTimeInterval target =
            static_cast<NSTimeInterval>(to) / 1000.0;
        if (target > m_duration) {
            return MUSIC_ERROR;
        }
        @autoreleasepool {
            setPositionLocked(target);
        }
        return kSuccess;
    }

    DWORD SetVolume(float value) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || !std::isfinite(value)) {
            return MUSIC_ERROR;
        }

        value = std::clamp(value, 0.0f, 1.0f);
        @autoreleasepool {
            if (m_kind == PlayerKind::Audio) {
                m_audioPlayer.volume = value;
            } else {
                m_audioEngine.mainMixerNode.outputVolume = value;
            }
        }
        return kSuccess;
    }

    DWORD Close() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_open && m_kind == PlayerKind::None) {
                return kSuccess;
            }
            m_exit = true;
        }
        m_condition.notify_all();
        if (m_controlThread.joinable()) {
            m_controlThread.join();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        @autoreleasepool {
            stopLocked();
            releasePlayersLocked();
            m_open   = false;
            m_status = MUSIC_MODE_NOT_OPEN;
        }
        return kSuccess;
    }

    DWORD GetPosition() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_ERROR;
        }
        @autoreleasepool {
            return millisecondsFromSeconds(currentPositionLocked());
        }
    }

    DWORD GetLength() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open ? millisecondsFromSeconds(m_duration) : MUSIC_ERROR;
    }

    DWORD GetPlayStatus() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return MUSIC_MODE_NOT_OPEN;
        }
        @autoreleasepool {
            if (m_status == MUSIC_MODE_PLAY && !isPlayingLocked() &&
                !m_repeat) {
                m_status = MUSIC_MODE_STOP;
            }
        }
        return m_status;
    }

private:
    enum class PlayerKind
    {
        None,
        Audio,
        MIDI
    };

    bool openMIDI(NSURL* fileURL)
    {
        AudioComponentDescription description{};
        description.componentType         = kAudioUnitType_MusicDevice;
        description.componentSubType      = kAudioUnitSubType_DLSSynth;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;

        m_audioEngine = [[AVAudioEngine alloc] init];
        m_midiSynth =
            [[AVAudioUnitMIDIInstrument alloc]
                initWithAudioComponentDescription:description];
        if (m_audioEngine == nil || m_midiSynth == nil) {
            return false;
        }

        [m_audioEngine attachNode:m_midiSynth];
        [m_audioEngine connect:m_midiSynth
                            to:m_audioEngine.mainMixerNode
                        format:nil];

        m_sequencer =
            [[AVAudioSequencer alloc] initWithAudioEngine:m_audioEngine];
        NSError* error = nil;
        if (m_sequencer == nil ||
            ![m_sequencer
                loadFromURL:fileURL
                    options:AVMusicSequenceLoadSMF_ChannelsToTracks
                      error:&error]) {
            return false;
        }

        NSTimeInterval duration = 0;
        for (AVMusicTrack* track in m_sequencer.tracks) {
            track.destinationAudioUnit = m_midiSynth;
            duration = std::max<NSTimeInterval>(
                duration, track.lengthInSeconds);
        }
        if (!(duration > 0)) {
            return false;
        }

        [m_sequencer prepareToPlay];
        [m_audioEngine prepare];
        m_duration = duration;
        m_kind     = PlayerKind::MIDI;
        return true;
    }

    bool startLocked()
    {
        if (m_kind == PlayerKind::Audio) {
            m_audioPlayer.numberOfLoops = 0;
            return [m_audioPlayer play] != FALSE;
        }

        NSError* error = nil;
        if (!m_audioEngine.isRunning &&
            ![m_audioEngine startAndReturnError:&error]) {
            return false;
        }
        error = nil;
        return [m_sequencer startAndReturnError:&error] != FALSE;
    }

    void stopLocked()
    {
        if (m_kind == PlayerKind::Audio) {
            [m_audioPlayer pause];
        } else if (m_kind == PlayerKind::MIDI) {
            const NSTimeInterval position =
                m_sequencer.currentPositionInSeconds;
            [m_sequencer stop];
            m_sequencer.currentPositionInSeconds = position;
        }
    }

    bool isPlayingLocked() const
    {
        if (m_kind == PlayerKind::Audio) {
            return m_audioPlayer.isPlaying != FALSE;
        }
        return m_kind == PlayerKind::MIDI &&
               m_sequencer.isPlaying != FALSE;
    }

    NSTimeInterval currentPositionLocked() const
    {
        if (m_kind == PlayerKind::Audio) {
            return m_audioPlayer.currentTime;
        }
        return m_kind == PlayerKind::MIDI
                   ? m_sequencer.currentPositionInSeconds
                   : 0;
    }

    void setPositionLocked(NSTimeInterval position)
    {
        position = std::clamp<NSTimeInterval>(position, 0, m_duration);
        if (m_kind == PlayerKind::Audio) {
            m_audioPlayer.currentTime = position;
        } else if (m_kind == PlayerKind::MIDI) {
            m_sequencer.currentPositionInSeconds = position;
        }
    }

    void controlLoop()
    {
        @autoreleasepool {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (!m_exit) {
                m_condition.wait_for(lock, std::chrono::milliseconds(5),
                                     [this] { return m_exit; });
                if (m_exit || !m_open ||
                    m_status != MUSIC_MODE_PLAY) {
                    continue;
                }

                @autoreleasepool {
                    const NSTimeInterval position =
                        currentPositionLocked();
                    if (position < m_rangeEnd && isPlayingLocked()) {
                        continue;
                    }

                    stopLocked();
                    if (m_repeat) {
                        setPositionLocked(m_rangeStart);
                        if (!startLocked()) {
                            m_status = MUSIC_MODE_NOT_READY;
                        }
                    } else {
                        setPositionLocked(m_rangeEnd);
                        m_status = MUSIC_MODE_STOP;
                    }
                }
            }
        }
    }

    void releasePlayersLocked()
    {
        if (m_sequencer != nil) {
            [m_sequencer stop];
        }
        if (m_audioEngine != nil) {
            [m_audioEngine stop];
        }
        if (m_audioPlayer != nil) {
            [m_audioPlayer stop];
        }

        m_sequencer   = nil;
        m_midiSynth   = nil;
        m_audioEngine = nil;
        m_audioPlayer = nil;
        m_kind        = PlayerKind::None;
    }

    AVAudioPlayer*             m_audioPlayer = nil;
    AVAudioEngine*             m_audioEngine = nil;
    AVAudioUnitMIDIInstrument* m_midiSynth   = nil;
    AVAudioSequencer*          m_sequencer   = nil;

    std::mutex              m_mutex;
    std::condition_variable m_condition;
    std::thread             m_controlThread;

    NSTimeInterval m_duration   = 0;
    NSTimeInterval m_rangeStart = 0;
    NSTimeInterval m_rangeEnd   = 0;
    DWORD          m_status     = MUSIC_MODE_NOT_OPEN;
    PlayerKind     m_kind       = PlayerKind::None;
    bool           m_open       = false;
    bool           m_exit       = false;
    bool           m_repeat     = false;
};

} // namespace

std::unique_ptr<MusicBackend> CreateMacOSMusicBackend()
{
    return std::make_unique<MacOSMusicBackend>();
}

} // namespace detail
} // namespace ege

#endif
