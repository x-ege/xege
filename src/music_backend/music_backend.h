#pragma once

#include "ege.h"

#include <memory>
#include <string>

namespace ege
{
namespace detail
{

/*
 * Private implementation contract for MUSIC.
 *
 * MUSIC is an old public class whose two data members are part of its ABI.
 * Platform backends therefore live outside the public object and are kept in
 * a registry owned by music.cpp.
 */
class MusicBackend
{
public:
    virtual ~MusicBackend() = default;

    virtual DWORD Open(const std::string& path) = 0;
    virtual DWORD Play(DWORD from, DWORD to, bool repeat) = 0;
    virtual DWORD Pause() = 0;
    virtual DWORD Stop() = 0;
    virtual DWORD Seek(DWORD to) = 0;
    virtual DWORD SetVolume(float value) = 0;
    virtual DWORD Close() = 0;
    virtual DWORD GetPosition() = 0;
    virtual DWORD GetLength() = 0;
    virtual DWORD GetPlayStatus() = 0;
};

#if defined(__APPLE__)
std::unique_ptr<MusicBackend> CreateMacOSMusicBackend();
#elif defined(__linux__)
#if defined(EGE_MUSIC_HAS_GSTREAMER)
std::unique_ptr<MusicBackend> CreateGStreamerMusicBackend();
#endif
std::unique_ptr<MusicBackend> CreateMiniaudioMusicBackend();
#endif

} // namespace detail
} // namespace ege
