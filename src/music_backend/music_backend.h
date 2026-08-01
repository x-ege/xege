#pragma once

#include <cstdint>
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
 * a registry owned by music.cpp. Keep this header independent of ege.h so
 * Objective-C++ backends can include Apple framework headers without
 * colliding with EGE's Win32-compatible BOOL typedef.
 */
class MusicBackend
{
public:
    virtual ~MusicBackend() = default;

    virtual std::uint32_t Open(const std::string& path) = 0;
    virtual std::uint32_t Play(std::uint32_t from, std::uint32_t to,
                               bool repeat) = 0;
    virtual std::uint32_t Pause() = 0;
    virtual std::uint32_t Stop() = 0;
    virtual std::uint32_t Seek(std::uint32_t to) = 0;
    virtual std::uint32_t SetVolume(float value) = 0;
    virtual std::uint32_t Close() = 0;
    virtual std::uint32_t GetPosition() = 0;
    virtual std::uint32_t GetLength() = 0;
    virtual std::uint32_t GetPlayStatus() = 0;
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
