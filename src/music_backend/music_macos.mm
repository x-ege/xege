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

using MusicResult = std::uint32_t;

constexpr MusicResult kSuccess           = 0;
constexpr MusicResult kMusicError        = UINT32_MAX;
constexpr MusicResult kMusicModeNotOpen  = 0x0;
constexpr MusicResult kMusicModeNotReady = 0x20C;
constexpr MusicResult kMusicModePause    = 0x211;
constexpr MusicResult kMusicModePlay     = 0x20E;
constexpr MusicResult kMusicModeStop     = 0x20D;

MusicResult millisecondsFromSeconds(NSTimeInterval seconds)
{
    if (!std::isfinite(seconds) || seconds < 0) {
        return kMusicError;
    }
    const double milliseconds = seconds * 1000.0;
    return static_cast<MusicResult>(
        std::min<double>(milliseconds, kMusicError - 1));
}

bool hasMIDIExtension(NSURL* fileURL)
{
    NSString* extension = fileURL.pathExtension.lowercaseString;
    return [extension isEqualToString:@"mid"] ||
           [extension isEqualToString:@"midi"] ||
           [extension isEqualToString:@"kar"];
}

class MacOSMusicBackend final : public MusicBackend
{
public:
    MacOSMusicBackend() = default;

    ~MacOSMusicBackend() override
    {
        Close();
    }

    MusicResult Open(const std::string& path) override
    {
        if (path.empty()) {
            return kMusicError;
        }

        @autoreleasepool {
            @try {
                NSString* filePath =
                    [[NSString alloc] initWithUTF8String:path.c_str()];
                if (filePath == nil) {
                    return kMusicError;
                }
                NSURL* fileURL = [NSURL fileURLWithPath:filePath];

                NSError* error = nil;
                m_audioPlayer =
                    [[AVAudioPlayer alloc] initWithContentsOfURL:fileURL
                                                          error:&error];
                if (m_audioPlayer != nil) {
                    // Loading metadata is enough to open a file. prepareToPlay
                    // may legitimately fail in a headless session or when no
                    // output device is present; Play() will report a runtime
                    // device failure later if playback is requested.
                    (void)[m_audioPlayer prepareToPlay];
                    m_kind     = PlayerKind::Audio;
                    m_duration = m_audioPlayer.duration;
                } else {
                    m_audioPlayer = nil;
                    // AVAudioPlayer failures are not evidence that a file is
                    // MIDI. Initialising the DLS synthesizer for arbitrary or
                    // corrupt input can raise an Objective-C exception.
                    if (!hasMIDIExtension(fileURL) || !openMIDI(fileURL)) {
                        releasePlayersLocked();
                        return kMusicError;
                    }
                }

                if (!(m_duration > 0) || !std::isfinite(m_duration)) {
                    releasePlayersLocked();
                    return kMusicError;
                }

                m_status = kMusicModeStop;
                m_open   = true;
                m_exit   = false;
                try {
                    m_controlThread =
                        std::thread(&MacOSMusicBackend::controlLoop, this);
                } catch (...) {
                    m_open = false;
                    releasePlayersLocked();
                    return kMusicError;
                }
            } @catch (NSException*) {
                // Public MUSIC APIs report failure with MUSIC_ERROR. Never let
                // malformed input or unavailable AVFoundation components abort
                // the caller's process.
                m_audioPlayer = nil;
                m_audioEngine = nil;
                m_midiSynth   = nil;
                m_sequencer   = nil;
                m_kind        = PlayerKind::None;
                m_duration    = 0;
                m_status      = kMusicModeNotOpen;
                m_open        = false;
                return kMusicError;
            }
        }
        return kSuccess;
    }

    MusicResult Play(MusicResult from, MusicResult to, bool repeat) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicError;
        }

        @autoreleasepool {
            @try {
                const bool defaultFullLoop =
                    repeat && from == kMusicError && to == kMusicError;
                NSTimeInterval start =
                    from == kMusicError
                        ? (defaultFullLoop ? 0 : currentPositionLocked())
                        : static_cast<NSTimeInterval>(from) / 1000.0;
                NSTimeInterval end =
                    to == kMusicError
                        ? m_duration
                        : static_cast<NSTimeInterval>(to) / 1000.0;
                start = std::clamp<NSTimeInterval>(start, 0, m_duration);
                end   = std::clamp<NSTimeInterval>(end, 0, m_duration);
                if (start >= end) {
                    return kMusicError;
                }

                if (from != kMusicError || defaultFullLoop ||
                    currentPositionLocked() >= m_duration) {
                    setPositionLocked(start);
                }

                m_rangeStart = start;
                m_rangeEnd   = end;
                m_repeat     = repeat;
                if (!startLocked()) {
                    return kMusicError;
                }

                m_status = kMusicModePlay;
                m_condition.notify_all();
                return kSuccess;
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                m_repeat = false;
                return kMusicError;
            }
        }
    }

    MusicResult Pause() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicError;
        }
        @autoreleasepool {
            @try {
                stopLocked();
                m_status = kMusicModePause;
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                return kMusicError;
            }
        }
        return kSuccess;
    }

    MusicResult Stop() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicError;
        }
        @autoreleasepool {
            @try {
                stopLocked();
                m_repeat = false;
                m_status = kMusicModeStop;
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                m_repeat = false;
                return kMusicError;
            }
        }
        return kSuccess;
    }

    MusicResult Seek(MusicResult to) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicError;
        }

        const NSTimeInterval target =
            static_cast<NSTimeInterval>(to) / 1000.0;
        if (target > m_duration) {
            return kMusicError;
        }
        @autoreleasepool {
            @try {
                setPositionLocked(target);
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                return kMusicError;
            }
        }
        return kSuccess;
    }

    MusicResult SetVolume(float value) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open || !std::isfinite(value)) {
            return kMusicError;
        }

        value = std::clamp(value, 0.0f, 1.0f);
        @autoreleasepool {
            @try {
                if (m_kind == PlayerKind::Audio) {
                    m_audioPlayer.volume = value;
                } else {
                    m_audioEngine.mainMixerNode.outputVolume = value;
                }
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                return kMusicError;
            }
        }
        return kSuccess;
    }

    MusicResult Close() override
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

        MusicResult result = kSuccess;
        std::lock_guard<std::mutex> lock(m_mutex);
        @autoreleasepool {
            @try {
                stopLocked();
                releasePlayersLocked();
            } @catch (NSException*) {
                // ARC assignments below still release every retained object;
                // do not let framework teardown replace a program's exit path.
                m_sequencer   = nil;
                m_midiSynth   = nil;
                m_audioEngine = nil;
                m_audioPlayer = nil;
                m_kind        = PlayerKind::None;
                result        = kMusicError;
            }
            m_open   = false;
            m_status = kMusicModeNotOpen;
            m_repeat = false;
        }
        return result;
    }

    MusicResult GetPosition() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicError;
        }
        @autoreleasepool {
            @try {
                return millisecondsFromSeconds(currentPositionLocked());
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
                return kMusicError;
            }
        }
    }

    MusicResult GetLength() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_open ? millisecondsFromSeconds(m_duration) : kMusicError;
    }

    MusicResult GetPlayStatus() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_open) {
            return kMusicModeNotOpen;
        }
        @autoreleasepool {
            @try {
                if (m_status == kMusicModePlay && !isPlayingLocked() &&
                    !m_repeat) {
                    m_status = kMusicModeStop;
                }
            } @catch (NSException*) {
                m_status = kMusicModeNotReady;
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

        if (AudioComponentFindNext(nullptr, &description) == nullptr) {
            return false;
        }

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
                    m_status != kMusicModePlay) {
                    continue;
                }

                @autoreleasepool {
                    @try {
                        const NSTimeInterval position =
                            currentPositionLocked();
                        if (position < m_rangeEnd && isPlayingLocked()) {
                            continue;
                        }

                        stopLocked();
                        if (m_repeat) {
                            setPositionLocked(m_rangeStart);
                            if (!startLocked()) {
                                m_status = kMusicModeNotReady;
                            }
                        } else {
                            setPositionLocked(m_rangeEnd);
                            m_status = kMusicModeStop;
                        }
                    } @catch (NSException*) {
                        m_status = kMusicModeNotReady;
                        m_repeat = false;
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
    MusicResult    m_status     = kMusicModeNotOpen;
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
