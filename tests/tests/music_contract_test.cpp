#include "ege.h"

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

} // namespace

int main()
{
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
           "narrow OpenFile reports the unavailable Unix backend");
    expect(music.OpenFile(L"/path/that/does/not/exist.mp3") == MUSIC_ERROR,
           "wide OpenFile reports the unavailable Unix backend");
    expect(music.IsOpen() == 0,
           "a failed Unix OpenFile does not mark the object open");
#endif

    if (failures != 0) {
        std::cerr << failures << " MUSIC contract assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All MUSIC contract assertions passed\n";
    return EXIT_SUCCESS;
}
