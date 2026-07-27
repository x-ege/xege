#include "ege.h"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

namespace {

int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void writeU16(std::ofstream& output, std::uint16_t value)
{
    output.put(static_cast<char>(value & 0xFF));
    output.put(static_cast<char>((value >> 8) & 0xFF));
}

void writeU32(std::ofstream& output, std::uint32_t value)
{
    writeU16(output, static_cast<std::uint16_t>(value & 0xFFFF));
    writeU16(output, static_cast<std::uint16_t>(value >> 16));
}

bool writeWaveFixture(const std::string& path)
{
    constexpr std::uint32_t sampleRate = 8000;
    constexpr std::uint32_t frameCount = sampleRate;
    constexpr std::uint32_t dataSize   = frameCount * 2;

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }

    output.write("RIFF", 4);
    writeU32(output, 36 + dataSize);
    output.write("WAVEfmt ", 8);
    writeU32(output, 16);
    writeU16(output, 1); // PCM
    writeU16(output, 1); // mono
    writeU32(output, sampleRate);
    writeU32(output, sampleRate * 2);
    writeU16(output, 2);
    writeU16(output, 16);
    output.write("data", 4);
    writeU32(output, dataSize);

    for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
        // A low-volume square wave avoids introducing floating-point fixture
        // generation differences between compilers.
        const std::int16_t sample =
            (frame / 20) % 2 == 0 ? 2000 : -2000;
        writeU16(output, static_cast<std::uint16_t>(sample));
    }
    return output.good();
}

bool writeMidiFixture(const std::string& path)
{
    // Format 0, one 96-tick quarter note, followed by end-of-track.
    static const unsigned char midi[] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
        'M', 'T', 'r', 'k', 0, 0, 0, 15,
        0, 0xC0, 0,
        0, 0x90, 60, 64,
        96, 0x80, 60, 64,
        0, 0xFF, 0x2F, 0
    };

    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(midi), sizeof(midi));
    return output.good();
}

bool positionEventuallyAtLeast(ege::MUSIC& music, DWORD expected)
{
    for (int attempt = 0; attempt < 40; ++attempt) {
        const DWORD position = music.GetPosition();
        if (position != MUSIC_ERROR && position >= expected) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

} // namespace

int main()
{
#ifndef _WIN32
    setenv("EGE_MUSIC_AUDIO_BACKEND", "null", 1);
#endif

    ege::MUSIC music;
    expect(music.IsOpen() == 0, "a new MUSIC object is closed");
    expect(music.GetPlayStatus() == ege::MUSIC_MODE_NOT_OPEN,
           "a new MUSIC object reports not-open status");
    expect(music.Play() == MUSIC_ERROR, "Play rejects an unopened object");
    expect(music.Play(0, 10) == MUSIC_ERROR,
           "the ranged Play overload rejects an unopened object");
    expect(music.RepeatPlay() == MUSIC_ERROR,
           "RepeatPlay rejects an unopened object");
    expect(music.RepeatPlay(0, 10) == MUSIC_ERROR,
           "the ranged RepeatPlay overload rejects an unopened object");
    expect(music.Pause() == MUSIC_ERROR, "Pause rejects an unopened object");
    expect(music.Stop() == MUSIC_ERROR, "Stop rejects an unopened object");
    expect(music.SetVolume(0.5f) == MUSIC_ERROR,
           "SetVolume rejects an unopened object");
    expect(music.Seek(0) == MUSIC_ERROR, "Seek rejects an unopened object");
    expect(music.GetPosition() == MUSIC_ERROR,
           "GetPosition does not manufacture a valid position");
    expect(music.GetLength() == MUSIC_ERROR,
           "GetLength does not manufacture a valid duration");
    expect(music.Close() == 0,
           "Close is idempotent for an unopened MUSIC object");

#ifndef _WIN32
    expect(music.OpenFile("/path/that/does/not/exist.mp3") == MUSIC_ERROR,
           "narrow OpenFile rejects a missing Unix file");
    expect(music.OpenFile(L"/path/that/does/not/exist.mp3") == MUSIC_ERROR,
           "wide OpenFile rejects a missing Unix file");
    expect(music.IsOpen() == 0,
           "a failed Unix OpenFile does not mark the object open");

    const bool forceMiniaudio =
        std::getenv("EGE_MUSIC_BACKEND") != nullptr;
    const std::string fixturePath =
        forceMiniaudio ? "music_contract_miniaudio_fixture.wav"
                       : "music_contract_fixture.wav";
    expect(writeWaveFixture(fixturePath),
           "the generated PCM WAV fixture can be written");
    if (music.OpenFile(fixturePath.c_str()) != 0) {
        expect(false, "the Unix backend opens a valid PCM WAV file");
    } else {
        expect(music.IsOpen() == 1,
               "a successful OpenFile marks the MUSIC object open");
        const DWORD length = music.GetLength();
        expect(length >= 950 && length <= 1050,
               "GetLength reports the generated one-second WAV");
        expect(music.GetPlayStatus() == ege::MUSIC_MODE_STOP,
               "a newly opened file starts in stopped state");
        expect(music.SetVolume(0.25f) == 0,
               "SetVolume controls an opened Unix backend");
        expect(music.Seek(250) == 0,
               "Seek accepts a position inside the file");
        expect(positionEventuallyAtLeast(music, 240),
               "GetPosition reflects a successful Seek");

        expect(music.Play(100, 700) == 0,
               "Play supports a bounded millisecond range");
        expect(music.GetPlayStatus() == ege::MUSIC_MODE_PLAY,
               "Play changes the state to playing");
        expect(positionEventuallyAtLeast(music, 115),
               "bounded playback advances from its start position");

        expect(music.Pause() == 0, "Pause succeeds while playing");
        expect(music.GetPlayStatus() == ege::MUSIC_MODE_PAUSE,
               "Pause changes the state to paused");
        const DWORD pausedPosition = music.GetPosition();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        const DWORD pausedPositionLater = music.GetPosition();
        expect(pausedPosition != MUSIC_ERROR &&
                   pausedPositionLater != MUSIC_ERROR &&
                   pausedPositionLater <= pausedPosition + 25,
               "Pause keeps the playback position stable");

        expect(music.RepeatPlay(100, 220) == 0,
               "RepeatPlay accepts a bounded millisecond range");
        std::this_thread::sleep_for(std::chrono::milliseconds(280));
        const DWORD repeatedPosition = music.GetPosition();
        expect(music.GetPlayStatus() == ege::MUSIC_MODE_PLAY,
               "RepeatPlay remains in playing state across a boundary");
        expect(repeatedPosition >= 90 && repeatedPosition <= 240,
               "RepeatPlay loops inside its requested range");

        expect(music.Stop() == 0, "Stop succeeds while playing");
        expect(music.GetPlayStatus() == ege::MUSIC_MODE_STOP,
               "Stop changes the state to stopped");
        expect(music.Close() == 0, "Close releases an opened Unix backend");
        expect(music.IsOpen() == 0, "Close marks the MUSIC object closed");
        expect(music.Close() == 0, "Close remains idempotent after playback");
    }

    const std::wstring wideFixturePath(fixturePath.begin(),
                                       fixturePath.end());
    expect(music.OpenFile(wideFixturePath.c_str()) == 0,
           "wide OpenFile opens a valid Unix path");
    expect(music.Close() == 0,
           "Close releases a file opened through the wide overload");
    std::remove(fixturePath.c_str());

    bool testMidi = false;
#if defined(__APPLE__)
    testMidi = true;
#elif defined(__linux__)
    testMidi = std::getenv("EGE_TEST_MUSIC_MIDI") != nullptr;
#endif
    if (testMidi) {
        const std::string midiPath = "music_contract_fixture.mid";
        expect(writeMidiFixture(midiPath),
               "the generated Standard MIDI fixture can be written");
        if (music.OpenFile(midiPath.c_str()) != 0) {
            expect(false, "the platform MIDI backend opens a valid SMF file");
        } else {
            const DWORD midiLength = music.GetLength();
            expect(midiLength >= 400 && midiLength <= 700,
                   "MIDI duration follows its quarter-note timeline");
            expect(music.SetVolume(0.25f) == 0,
                   "MIDI playback exposes real volume control");
            expect(music.Seek(100) == 0,
                   "MIDI playback supports millisecond seeking");
            expect(music.Play() == 0, "MIDI playback starts");
            expect(music.Pause() == 0, "MIDI playback pauses");
            expect(music.Close() == 0, "MIDI playback closes");
        }
        std::remove(midiPath.c_str());
    }
#endif

    if (failures != 0) {
        std::cerr << failures << " MUSIC contract assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MUSIC contract assertions passed\n";
    return EXIT_SUCCESS;
}
