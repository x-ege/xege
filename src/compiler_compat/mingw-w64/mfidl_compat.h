#pragma once

/*
 * mingw-w64 < 10.0.0 compatibility declarations for Media Foundation headers.
 *
 * mfidl.h in these toolchains does not declare MFCreateDeviceSource even though
 * mf.lib exports it.  The declaration was added to mfidl.idl in mingw-w64
 * v10.0.0; older MinGW toolchains therefore need this build-time shim.
 *
 * This header is private to the build and is force-included only into ccap's
 * Media Foundation translation unit.  MSVC and newer mingw-w64 headers never
 * see an active declaration here.
 */
#if defined(_WIN32) && defined(__MINGW32__) && \
    (!defined(__MINGW64_VERSION_MAJOR) || __MINGW64_VERSION_MAJOR < 10)

#include <mfapi.h>
#include <mfidl.h>

extern "C" HRESULT WINAPI MFCreateDeviceSource(IMFAttributes *pAttributes, IMFMediaSource **ppSource);

#endif
