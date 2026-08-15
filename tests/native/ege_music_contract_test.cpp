#include "test_support.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void appendLE16(std::vector<unsigned char>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8U));
}

void appendLE32(std::vector<unsigned char>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8U));
    bytes.push_back(static_cast<unsigned char>(value >> 16U));
    bytes.push_back(static_cast<unsigned char>(value >> 24U));
}

std::filesystem::path writeSilentWav()
{
    constexpr std::uint32_t sampleRate = 8000;
    constexpr std::uint32_t sampleCount = 800;
    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bitsPerSample = 16;
    constexpr std::uint32_t dataSize = sampleCount * channels * bitsPerSample / 8;

    std::vector<unsigned char> bytes;
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    appendLE32(bytes, 36 + dataSize);
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    appendLE32(bytes, 16);
    appendLE16(bytes, 1);
    appendLE16(bytes, channels);
    appendLE32(bytes, sampleRate);
    appendLE32(bytes, sampleRate * channels * bitsPerSample / 8);
    appendLE16(bytes, channels * bitsPerSample / 8);
    appendLE16(bytes, bitsPerSample);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    appendLE32(bytes, dataSize);
    bytes.resize(bytes.size() + dataSize, 0);

    const std::filesystem::path path = ege_test::artifacts() / "music-silence.wav";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    EGE_CHECK(output.good());
    return path;
}

void writeCorruptMidi(const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "not a midi file";
    EGE_CHECK(output.good());
}

} // namespace

int main()
{
    ege::MUSIC music;
    EGE_CHECK(music.OpenFile("/definitely/missing/xege-audio.wav") == MUSIC_ERROR);
    EGE_CHECK(!music.IsOpen());

    const std::filesystem::path wav = writeSilentWav();
    const std::filesystem::path corruptMidi = ege_test::artifacts() / "corrupt.mid";
    writeCorruptMidi(corruptMidi);
    EGE_CHECK(music.OpenFile(corruptMidi.string().c_str()) == MUSIC_ERROR);
    EGE_CHECK(!music.IsOpen());

    EGE_CHECK(music.OpenFile(wav.string().c_str()) == 0);
    EGE_CHECK(music.IsOpen());
    const DWORD length = music.GetLength();
    EGE_CHECK(length >= 90 && length <= 110);
    EGE_CHECK(music.GetPlayStatus() == MUSIC_MODE_STOP);
    EGE_CHECK(music.SetVolume(0.25f) == 0);
    EGE_CHECK(music.Seek(50) == 0);
    EGE_CHECK(music.Seek(length + 1) == MUSIC_ERROR);
    EGE_CHECK(music.Stop() == 0);
    EGE_CHECK(music.Close() == 0);
    EGE_CHECK(!music.IsOpen());

    const std::string narrowPath = wav.string();
    const std::wstring widePath(narrowPath.begin(), narrowPath.end());
    EGE_CHECK(music.OpenFile(widePath.c_str()) == 0);
    EGE_CHECK(music.Close() == 0);

    return ege_test::finish("MUSIC backend contract");
}
