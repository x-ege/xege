/*
filename  music.cpp

MUSIC类的定义
*/

#include "ege_head.h"
#include "ege_common.h"

#ifdef _WIN32
#include <mmsystem.h>
#include <digitalv.h>
#else
#include "music_backend/music_backend.h"

#include <cstdlib>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#endif

#ifndef MUSIC_ASSERT_TRUE
#   ifdef _DEBUG
#       include <cassert>
#       define MUSIC_ASSERT_TRUE(e) assert((e) != MUSIC_ERROR)
#   else
#       define MUSIC_ASSERT_TRUE(e) (void(0))
#   endif
#endif

namespace ege
{

// Class MUSIC Construction
#ifdef _WIN32
MUSIC::MUSIC()
{
    m_DID        = MUSIC_ERROR;
    m_dwCallBack = 0;
    dll::loadWinmmDll();
}

// Class MUSIC Destruction
MUSIC::~MUSIC()
{
    if (m_DID != MUSIC_ERROR) {
        Close();
    }
}

// open a music file. szStr: Path of the file
DWORD MUSIC::OpenFile(const char* _szStr)
{
    const std::wstring& wszStr = mb2w(_szStr);
    return OpenFile(wszStr.c_str());
}

// open a music file. szStr: Path of the file
DWORD MUSIC::OpenFile(const wchar_t* _szStr)
{
    MCIERROR        mciERR = ERROR_SUCCESS;
    MCI_OPEN_PARMSW mci_p  = {0};

    mci_p.lpstrElementName = _szStr;
    mci_p.lpstrDeviceType  = NULL;
    mci_p.dwCallback       = (DWORD_PTR)m_dwCallBack;

    if (m_DID != MUSIC_ERROR) {
        Close();
    }

    mciERR = dll::mciSendCommandW(0, MCI_OPEN, MCI_OPEN_SHAREABLE | MCI_NOTIFY | MCI_OPEN_ELEMENT, (DWORD_PTR)&mci_p);

    if (mciERR != ERROR_SUCCESS) {
        mciERR = dll::mciSendCommandW(0, MCI_OPEN, MCI_NOTIFY | MCI_OPEN_ELEMENT, (DWORD_PTR)&mci_p);
    }

    if (mciERR == ERROR_SUCCESS) {
        m_DID = mci_p.wDeviceID;

        // Set time format with milliseconds
        {
            MCI_SET_PARMS mci_p = {0};
            mci_p.dwTimeFormat  = MCI_FORMAT_MILLISECONDS;
            // DWORD dw =
            dll::mciSendCommandW(m_DID, MCI_SET, MCI_NOTIFY | MCI_SET_TIME_FORMAT, (DWORD_PTR)&mci_p);
        }
    }

    return mciERR;
}

// mciPlay(DWORD dwFrom, DWORD dwTo, DWORD dwCallBack)
// play the music stream.
DWORD MUSIC::Play(DWORD dwFrom, DWORD dwTo)
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR       mciERR = ERROR_SUCCESS;
    MCI_PLAY_PARMS mci_p  = {0};
    DWORD          dwFlag = MCI_NOTIFY;

    mci_p.dwFrom     = dwFrom;
    mci_p.dwTo       = dwTo;
    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;

    if (dwFrom != MUSIC_ERROR) {
        dwFlag |= MCI_FROM;
    }

    if (dwTo != MUSIC_ERROR) {
        dwFlag |= MCI_TO;
    }

    mciERR = dll::mciSendCommandW(m_DID, MCI_PLAY, dwFlag, (DWORD_PTR)&mci_p);

    return mciERR;
}

DWORD MUSIC::RepeatPlay(DWORD dwFrom, DWORD dwTo)
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR       mciERR = ERROR_SUCCESS;
    MCI_PLAY_PARMS mci_p  = {0};
    DWORD          dwFlag = MCI_NOTIFY | MCI_DGV_PLAY_REPEAT;

    mci_p.dwFrom     = dwFrom;
    mci_p.dwTo       = dwTo;
    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;

    if (dwFrom != MUSIC_ERROR) {
        dwFlag |= MCI_FROM;
    }

    if (dwTo != MUSIC_ERROR) {
        dwFlag |= MCI_TO;
    }

    mciERR = dll::mciSendCommandW(m_DID, MCI_PLAY, dwFlag, (DWORD_PTR)&mci_p);

    return mciERR;
}

// pause the music stream.
DWORD MUSIC::Pause()
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR          mciERR = ERROR_SUCCESS;
    MCI_GENERIC_PARMS mci_p  = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;

    mciERR = dll::mciSendCommandW(m_DID, MCI_PAUSE, MCI_NOTIFY, (DWORD_PTR)&mci_p);

    return mciERR;
}

// stop the music stream.
DWORD MUSIC::Stop()
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR          mciERR = ERROR_SUCCESS;
    MCI_GENERIC_PARMS mci_p  = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;

    mciERR = dll::mciSendCommandW(m_DID, MCI_STOP, MCI_NOTIFY, (DWORD_PTR)&mci_p);

    return mciERR;
}

DWORD MUSIC::SetVolume(float value)
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR                mciERR = ERROR_SUCCESS;
    MCI_DGV_SETAUDIO_PARMSW mci_p  = {0};
    mci_p.dwItem                   = MCI_DGV_SETAUDIO_VOLUME;
    mci_p.dwValue                  = (DWORD)(value * 1000); // 此处就是音量大小 (0--1000)

    mciERR = dll::mciSendCommandW(m_DID, MCI_SETAUDIO, MCI_DGV_SETAUDIO_VALUE | MCI_DGV_SETAUDIO_ITEM, (DWORD_PTR)&mci_p);

    return mciERR;
}

// seek the music stream playposition to `dwTo`
DWORD MUSIC::Seek(DWORD dwTo)
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCIERROR       mciERR = ERROR_SUCCESS;
    MCI_SEEK_PARMS mci_p  = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;
    mci_p.dwTo       = dwTo;

    mciERR = dll::mciSendCommandW(m_DID, MCI_SEEK, MCI_NOTIFY, (DWORD_PTR)&mci_p);

    return mciERR;
}

// close the music stream.
DWORD MUSIC::Close()
{
    if (m_DID != MUSIC_ERROR) {
        MCIERROR          mciERR = ERROR_SUCCESS;
        MCI_GENERIC_PARMS mci_p  = {0};

        mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;

        mciERR = dll::mciSendCommandW(m_DID, MCI_CLOSE, MCI_NOTIFY, (DWORD_PTR)&mci_p);

        m_DID = MUSIC_ERROR;
        return mciERR;
    } else {
        return ERROR_SUCCESS;
    }
}

// get the playing position. return by milliseconds
DWORD MUSIC::GetPosition()
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCI_STATUS_PARMS mci_p = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;
    mci_p.dwItem     = MCI_STATUS_POSITION;

    dll::mciSendCommandW(m_DID, MCI_STATUS, MCI_NOTIFY | MCI_STATUS_ITEM, (DWORD_PTR)&mci_p);

    return (DWORD)mci_p.dwReturn;
}

// get the length of the music stream. return by milliseconds
DWORD MUSIC::GetLength()
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_ERROR;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCI_STATUS_PARMS mci_p = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;
    mci_p.dwItem     = MCI_STATUS_LENGTH;

    dll::mciSendCommandW(m_DID, MCI_STATUS, MCI_NOTIFY | MCI_STATUS_ITEM, (DWORD_PTR)&mci_p);

    return (DWORD)mci_p.dwReturn;
}

DWORD MUSIC::GetPlayStatus()
{
    if (m_DID == MUSIC_ERROR) {
        return MUSIC_MODE_NOT_OPEN;
    }
    MUSIC_ASSERT_TRUE(m_DID);
    MCI_STATUS_PARMS mci_p = {0};

    mci_p.dwCallback = (DWORD_PTR)m_dwCallBack;
    mci_p.dwItem     = MCI_STATUS_MODE;

    dll::mciSendCommandW(m_DID, MCI_STATUS, MCI_NOTIFY | MCI_STATUS_ITEM, (DWORD_PTR)&mci_p);

    return (DWORD)mci_p.dwReturn;
}
#else
namespace
{

using MusicBackendPtr = std::shared_ptr<detail::MusicBackend>;

std::mutex& musicRegistryMutex()
{
    // Process-lifetime storage keeps global MUSIC destructors safe regardless
    // of translation-unit static destruction order.
    static std::mutex* mutex = new std::mutex;
    return *mutex;
}

std::unordered_map<const MUSIC*, MusicBackendPtr>& musicRegistry()
{
    static auto* registry =
        new std::unordered_map<const MUSIC*, MusicBackendPtr>;
    return *registry;
}

MusicBackendPtr getMusicBackend(const MUSIC* music)
{
    std::lock_guard<std::mutex> lock(musicRegistryMutex());
    const auto                 it = musicRegistry().find(music);
    return it == musicRegistry().end() ? MusicBackendPtr() : it->second;
}

void setMusicBackend(const MUSIC* music, MusicBackendPtr backend)
{
    std::lock_guard<std::mutex> lock(musicRegistryMutex());
    musicRegistry()[music] = std::move(backend);
}

MusicBackendPtr removeMusicBackend(const MUSIC* music)
{
    std::lock_guard<std::mutex> lock(musicRegistryMutex());
    const auto                 it = musicRegistry().find(music);
    if (it == musicRegistry().end()) {
        return {};
    }

    MusicBackendPtr backend = std::move(it->second);
    musicRegistry().erase(it);
    return backend;
}

std::vector<std::unique_ptr<detail::MusicBackend>> createMusicBackends()
{
    std::vector<std::unique_ptr<detail::MusicBackend>> backends;
#if defined(__APPLE__)
    backends.emplace_back(detail::CreateMacOSMusicBackend());
#elif defined(__linux__)
#if defined(EGE_MUSIC_HAS_GSTREAMER)
    const char* preference = std::getenv("EGE_MUSIC_BACKEND");
    const bool  forceMiniaudio =
        preference != nullptr && std::string(preference) == "miniaudio";
    if (!forceMiniaudio) {
        backends.emplace_back(detail::CreateGStreamerMusicBackend());
    }
#endif
    backends.emplace_back(detail::CreateMiniaudioMusicBackend());
#endif
    return backends;
}

} // namespace

MUSIC::MUSIC()
{
    m_DID        = MUSIC_ERROR;
    m_dwCallBack = 0;
}

MUSIC::~MUSIC()
{
    Close();
}

DWORD MUSIC::OpenFile(const char* _szStr)
{
    if (_szStr == nullptr || *_szStr == '\0') {
        return MUSIC_ERROR;
    }

    if (m_DID != MUSIC_ERROR) {
        Close();
    }

    for (auto& backend : createMusicBackends()) {
        if (backend && backend->Open(_szStr) == ERROR_SUCCESS) {
            setMusicBackend(this, MusicBackendPtr(std::move(backend)));
            // The native handle stays outside the public object to preserve
            // MUSIC's ABI. Any non-error value marks a successful open.
            m_DID = 1;
            return ERROR_SUCCESS;
        }
    }

    return MUSIC_ERROR;
}

DWORD MUSIC::OpenFile(const wchar_t* _szStr)
{
    if (_szStr == nullptr || *_szStr == L'\0') {
        return MUSIC_ERROR;
    }
    const std::string path = w2utf8(_szStr);
    return path.empty() ? MUSIC_ERROR : OpenFile(path.c_str());
}

DWORD MUSIC::Play(DWORD dwFrom, DWORD dwTo)
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->Play(dwFrom, dwTo, false) : MUSIC_ERROR;
}

DWORD MUSIC::RepeatPlay(DWORD dwFrom, DWORD dwTo)
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->Play(dwFrom, dwTo, true) : MUSIC_ERROR;
}

DWORD MUSIC::Pause()
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->Pause() : MUSIC_ERROR;
}

DWORD MUSIC::Stop()
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->Stop() : MUSIC_ERROR;
}

DWORD MUSIC::SetVolume(float value)
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->SetVolume(value) : MUSIC_ERROR;
}

DWORD MUSIC::Seek(DWORD dwTo)
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->Seek(dwTo) : MUSIC_ERROR;
}

DWORD MUSIC::Close()
{
    MusicBackendPtr backend = removeMusicBackend(this);
    m_DID                   = MUSIC_ERROR;
    return backend ? backend->Close() : ERROR_SUCCESS;
}

DWORD MUSIC::GetPosition()
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->GetPosition() : MUSIC_ERROR;
}

DWORD MUSIC::GetLength()
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->GetLength() : MUSIC_ERROR;
}

DWORD MUSIC::GetPlayStatus()
{
    const MusicBackendPtr backend = getMusicBackend(this);
    return backend ? backend->GetPlayStatus() : MUSIC_MODE_NOT_OPEN;
}
#endif

} // namespace ege
