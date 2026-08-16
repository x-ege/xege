/*
* EGE (Easy Graphics Engine)
* filename  image_ex.cpp

本文件集中基于stb_image的对image基本操作的接口和类定义
*/

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include "ege_head.h"
#include "ege_common.h"
#include "ege_dllimport.h"

#include "image.h"
// #ifdef _ITERATOR_DEBUG_LEVEL
// #undef _ITERATOR_DEBUG_LEVEL
// #endif

#include "external/stb_image.h"
#include "external/stb_image_resize2.h"
#include "external/stb_image_write.h"
#include "stb_image_impl.h"

#include <math.h>
#include <limits.h>
#include <cstdio>
#include <cwctype>
#include <string>

namespace ege
{

static FILE* openWideFile(const wchar_t* filename, const wchar_t* mode)
{
#ifdef _WIN32
    return ::_wfopen(filename, mode);
#else
    if (filename == NULL || mode == NULL) {
        return NULL;
    }
    const std::string utf8Filename = w2mb(filename);
    const std::string utf8Mode = w2mb(mode);
    if (utf8Filename.empty() || utf8Mode.empty()) {
        return NULL;
    }
    return fopen(utf8Filename.c_str(), utf8Mode.c_str());
#endif
}

static graphics_errors convertStbImageError(const char* errorStr)
{
	graphics_errors error = grError;

	if (!isEmpty(errorStr)) {
		if (startsWith(errorStr, "can't fopen")) {
			error = grFileNotFound;
		} else if (startsWith(errorStr, "outofmem")) {
			error = grOutOfMemory;
		} else if (startsWith(errorStr, "too large") || startsWith(errorStr, "unsupported") ||
				   startsWith(errorStr, "unknown")   || startsWith(errorStr, "wrong")) {
			error = grUnsupportedFormat;
		} else if (startsWith(errorStr, "bad") || startsWith(errorStr, "invalid") || startsWith(errorStr, "corrupt") ||
				   startsWith(errorStr, "not") || startsWith(errorStr, "missing") || startsWith(errorStr, "illegal")){
			error = grInvalidFileFormat;
		}
	}

	return error;
}

graphics_errors getimage_from_memory_stb(PIMAGE image, const void* memory, long size)
{
    if (image == NULL || memory == NULL || size <= 0 || size > INT_MAX) {
        return grParamError;
    }

    int width = 0;
    int height = 0;
    int channelsInFile = 0;
    color_t* pixels = reinterpret_cast<color_t*>(stbi_load_from_memory(
        static_cast<const stbi_uc*>(memory), static_cast<int>(size),
        &width, &height, &channelsInFile, STBI_rgb_alpha));
    if (pixels == NULL) {
        return convertStbImageError(stbi_failure_reason());
    }

    graphics_errors error = grAllocError;
    if (resize_f(image, width, height) == grOk) {
        color_t* destination = getbuffer(image);
        ABGRToARGB(destination, pixels, width * height);
        image_premultiply(destination, width, height);
        error = grOk;
    }
    stbi_image_free(pixels);
    return error;
}

int IMAGE::getimage(const char* filename, int zoomWidth, int zoomHeight)
{
	if (isEmpty(filename))
		return grParamError;
	const std::wstring& filename_w = mb2w(filename);
	return getimage(filename_w.c_str(), zoomWidth, zoomHeight);
}

int IMAGE::getimage(const wchar_t* filename, int zoomWidth, int zoomHeight)
{
	inittest(L"IMAGE::getimage");

	if (isEmpty(filename) || zoomWidth < 0 || zoomHeight < 0)
		return grParamError;

	FILE* fp = openWideFile(filename, L"rb");
	if (fp == NULL)
		return grFileNotFound;

	graphics_errors error = grOk;

	int width = 0, height = 0;
	int channelsInFile = 0;

	/* 尝试使用 stb_image 加载图像(支持格式: PNG, BMP, JPEG, GIF, PSD, HDR, PGM, PPM, PNM, TGA)*/
	color_t* pixels = (color_t*)stbi_load_from_file(fp, &width, &height, &channelsInFile, STBI_rgb_alpha);
	if (pixels) {
		const int targetWidth = zoomWidth == 0 ? width : zoomWidth;
		const int targetHeight = zoomHeight == 0 ? height : zoomHeight;
		const size_t targetCount = static_cast<size_t>(targetWidth) * targetHeight;
		color_t* outputPixels = pixels;
		color_t* scaledPixels = NULL;
		if (targetWidth <= 0 || targetHeight <= 0 || targetCount > INT_MAX) {
			error = grParamError;
		} else if (targetWidth != width || targetHeight != height) {
			scaledPixels = static_cast<color_t*>(malloc(targetCount * sizeof(color_t)));
			if (scaledPixels == NULL) {
				error = grOutOfMemory;
			} else if (stbir_resize_uint8_srgb(
				reinterpret_cast<const unsigned char*>(pixels), width, height, 0,
				reinterpret_cast<unsigned char*>(scaledPixels), targetWidth, targetHeight, 0,
				STBIR_RGBA) == NULL) {
				error = grError;
			} else {
				outputPixels = scaledPixels;
			}
		}

		if (error == grOk && this->resize_f(targetWidth, targetHeight) == grOk) {
			/* stb_image 返回的像素颜色存储按字节从高到低依次为 ABGR，和 ege 的存储顺序 ARGB 不一致，需要交换 R 和 B 通道. */
			color_t* destination = getbuffer();
			if (destination != NULL) {
				ABGRToARGB(destination, outputPixels, static_cast<int>(targetCount));
				image_premultiply(destination, targetWidth, targetHeight);
			} else {
				error = grInvalidMemory;
			}
		} else if (error == grOk) {
			error = grAllocError;
		}
		free(scaledPixels);
		stbi_image_free(pixels);
	} else {
		/* 加载失败，将错误信息转换为相应的错误码 */
		error = convertStbImageError(stbi_failure_reason());
	}

	fclose(fp);

	/* 如图像格式不受 stb_image 支持或者 stb_image 认为格式错误，再次尝试使用 GDI+ 读取 */
	if (error == grUnsupportedFormat || error == grInvalidFileFormat) {
#ifdef EGE_GDIPLUS
		/* GDI+ 支持格式：BMP, GIF, JPEG, PNG, TIFF, Exif, WMF, EMF */
		Gdiplus::Bitmap bitmap(filename);

		/* GDI+ bug: GDI+ Bitmap 只会报 InvalidParameter 错误，无法得到具体错误类型信息 */
		if (bitmap.GetLastStatus() != Gdiplus::Ok) {
			/* 通过文件扩展名判断是否是支持解码的格式，格式支持为 grInvalidFileFormat，格式不支持则为 grUnsupportedFormat. */
			ImageFormat       imageFormat  = checkImageFormatByFileName(filename);
			ImageDecodeFormat decodeFormat = getImageDecodeFormat(imageFormat);

			error = (decodeFormat == ImageDecodeFormat_NULL) ? grUnsupportedFormat : grInvalidFileFormat;
		} else {
			/* 从 GDI+ Bitmap 中读取图像数据，写入 ege IMAGE 中*/
			error = getimage_from_bitmap(this, bitmap);
		}
#endif
	}

	return error;
}

int IMAGE::saveimage(const char* filename, bool withAlphaChannel) const
{
	return saveimage(mb2w(filename).c_str(), withAlphaChannel);
}

int IMAGE::saveimage(const wchar_t* filename, bool withAlphaChannel) const
{
	return ege::saveimage(this, filename, withAlphaChannel);
}

static color_t colorForOpaqueFileOutput(color_t color)
{
    return EGEGET_A(color) == 0 ? color : color_unpremultiply(color);
}

int IMAGE::savepngimg(FILE* fp, bool withAlphaChannel) const
{
	int channels = withAlphaChannel ? 4 : 3;

	int pixelCount = m_width * m_height;
	int stride = channels * m_width;
	uint8_t* buffer = (uint8_t*)malloc(channels * pixelCount);

	if (buffer == NULL ) {
		return grOutOfMemory;
	}

	const color_t* sourceBuffer = getbuffer();
	if (sourceBuffer == NULL) {
		free(buffer);
		return static_cast<int>(grInvalidMemory);
	}

	if (withAlphaChannel) {
		// 像素格式转换 (BGRABGRA --> RGBARGBA)
		image_unpremultiply((color_t*)buffer, sourceBuffer, m_width, m_height);
		ARGBToABGR((color_t*)buffer, (color_t*)buffer, pixelCount);
	} else {
		// 像素格式转换 (BGRABGRA --> RGBRGB)
		uint8_t* dst = buffer;
		const color_t* src = sourceBuffer;
		for (int i = 0; i < pixelCount; i++) {
			const color_t color = colorForOpaqueFileOutput(*src);
			dst[0] = EGEGET_R(color);
			dst[1] = EGEGET_G(color);
			dst[2] = EGEGET_B(color);

			dst += channels;
			src++;
		}
	}

	int result = stbi_write_png_to_func(stbi_write_to_FILE_func,fp, m_width, m_height, channels, buffer, stride);
	free(buffer);

	return result ? grOk : grError;
}

#define EGE_GETIMAGE_CHK_NULL(p)                                          \
    do {                                                                  \
        if (p == NULL)                                                    \
            internal_panic(L"Fatal Error: pass NULL to `ege::getimage`"); \
    } while (0)

int getimage(PIMAGE imgDest, const char* imageFile, int zoomWidth, int zoomHeight)
{
	EGE_GETIMAGE_CHK_NULL(imgDest);
	return imgDest->getimage(imageFile, zoomWidth, zoomHeight);
}

int getimage(PIMAGE imgDest, const wchar_t* imageFile, int zoomWidth, int zoomHeight)
{
	EGE_GETIMAGE_CHK_NULL(imgDest);
	return imgDest->getimage(imageFile, zoomWidth, zoomHeight);
}

static BOOL nocaseends(LPCWSTR suffix, LPCWSTR text)
{
	int     len_suffix, len_text;
	LPCWSTR p_suffix;
	LPCWSTR p_text;
	len_suffix = (int)wcslen(suffix);
	len_text   = (int)wcslen(text);

	if ((len_text < len_suffix) || (len_text == 0)) {
		return FALSE;
	}

	p_suffix = suffix;
	p_text   = (text + (len_text - len_suffix));

	while (*p_text != 0) {
		if (towupper(*p_text) != towupper(*p_suffix)) {
			return FALSE;
		}
		p_text++;
		p_suffix++;
	}

	return TRUE;
}

int saveimage(PCIMAGE pimg, const char* filename, bool withAlphaChannel)
{
	if (isEmpty(filename))
		return grParamError;
	const std::wstring& filename_w = mb2w(filename);
	return saveimage(pimg, filename_w.c_str(), withAlphaChannel);
}

int saveimage(PCIMAGE pimg, const wchar_t* filename, bool withAlphaChannel)
{
	if (isEmpty(filename))
		return grParamError;
	PCIMAGE img = CONVERT_IMAGE_CONST(pimg);
	int     ret = 0;

	if (img) {
		if (nocaseends(L".bmp", filename)) {
			ret = savebmp(pimg, filename, withAlphaChannel);
		} else if (nocaseends(L".png", filename)) {
			ret = savepng(pimg, filename, withAlphaChannel);
		} else {
			ret = savepng(pimg, filename, withAlphaChannel);
		}
	}

	CONVERT_IMAGE_END;
	return ret;
}

int getimage_pngfile(PIMAGE pimg, const char* filename)
{
	if (isEmpty(filename))
		return grParamError;
	const std::wstring& filename_w = mb2w(filename);
	return getimage_pngfile(pimg, filename_w.c_str());
}

int getimage_pngfile(PIMAGE pimg, const wchar_t* filename)
{
	return getimage(pimg, filename);
}

int savepng(PCIMAGE pimg, const char* filename, bool withAlphaChannel)
{
	if (isEmpty(filename))
		return grParamError;
	const std::wstring& filename_w = mb2w(filename);
	return savepng(pimg, filename_w.c_str(), withAlphaChannel);
}

int savepng(PCIMAGE pimg, const wchar_t* filename, bool withAlphaChannel)
{
	if (isEmpty(filename))
		return grParamError;

	pimg = CONVERT_IMAGE_CONST(pimg);
	if (pimg == NULL) {
		return grParamError;
	}
	FILE* fp = openWideFile(filename, L"wb");

	if (fp == NULL) {
		return grIOerror;
	}

	int ret = pimg->savepngimg(fp, withAlphaChannel);
	fclose(fp);
	return ret;
}

} // namespace ege
