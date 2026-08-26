#pragma once

#include "ege_head.h"

#include <windows.h>


namespace ege
{

/*
                      支持的图像解码格式
 ┌───────────┬────────────────────────────────────────────────────────────────────┐
 │ stb_image │ PNG, BMP, JPEG, GIF,                     , PSD, HDR, PGM, PPM, TGA │
 ├───────────┼────────────────────────────────────────────────────────────────────┤
 │ GDI+      │ PNG, BMP, JPEG, GIF, TIFF, EXIF, WMF, EMF                          │
 └───────────┴────────────────────────────────────────────────────────────────────┘

                      支持的图像编码格式
 ┌───────────┬───────────────────────────────────────────────┐
 │ stb_image │ PNG, BMP,                                     │
 ├───────────┼───────────────────────────────────────────────┤
 │ GDI+      │ PNG, BMP, JPEG, GIF,                          │
 └───────────┴───────────────────────────────────────────────┘
 */

// 图像格式
enum ImageFormat
{
    ImageFormat_NULL = 0,
    ImageFormat_PNG,
    ImageFormat_BMP,
    ImageFormat_JPEG,
    ImageFormat_GIF,
    ImageFormat_TIFF,
    ImageFormat_EXIF,
    ImageFormat_WMF,
    ImageFormat_EMF,
    ImageFormat_PSD,
    ImageFormat_HDR,
    ImageFormat_PGM,
    ImageFormat_PPM,
    ImageFormat_TGA
};

// 图像解码格式
enum ImageDecodeFormat
{
    ImageDecodeFormat_NULL = 0,
    ImageDecodeFormat_PNG,
    ImageDecodeFormat_BMP,
    ImageDecodeFormat_JPEG,
    ImageDecodeFormat_GIF,
    ImageDecodeFormat_TIFF,
    ImageDecodeFormat_EXIF,
    ImageDecodeFormat_WMF,
    ImageDecodeFormat_EMF,
    ImageDecodeFormat_PSD,
    ImageDecodeFormat_HDR,
    ImageDecodeFormat_PGM,
    ImageDecodeFormat_PPM,
    ImageDecodeFormat_TGA
};

// 支持的图像编码格式
enum ImageEncodeFormat
{
    ImageEncodeFormat_NULL = 0,
    ImageEncodeFormat_PNG,
    ImageEncodeFormat_BMP
};

// 定义图像对象
class IMAGE
{
public:
    /* 初始颜色配置 */
    static const color_t initial_line_color = LIGHTGRAY;
    static const color_t initial_text_color = LIGHTGRAY;
    static const color_t initial_fill_color = BLACK;
    static const color_t initial_bk_color   = BLACK;
private:
    int m_initflag;

public:
    HDC     m_hDC;
    HBITMAP m_hBmp;
    int     m_width;
    int     m_height;
    PDWORD  m_pBuffer;
    color_t m_linecolor;
    color_t m_fillcolor;
    color_t m_textcolor;
    color_t m_bk_color;

private:
#ifdef EGE_GDIPLUS
    Gdiplus::Graphics* m_graphics;
    Gdiplus::Pen*      m_pen;
    Gdiplus::Brush*    m_brush;
#endif
    bool m_aa;
    void initimage(HDC refDC, int width, int height);
    void construct(int width, int height);
    void construct(int width, int height, color_t color);
    void setdefaultattribute();
    int  deleteimage();
    void reset();

public:
    Bound            m_vpt;
    bool             m_enableclip;
    textsettingstype m_texttype;
    line_style_type  m_linestyle;
    float            m_linewidth;
    line_cap_type    m_linestartcap;
    line_cap_type    m_lineendcap;
    line_join_type   m_linejoin;
    float            m_linejoinmiterlimit;
    void*            m_texture;

private:
    void inittest(const WCHAR* strCallFunction = NULL) const;

public:
    IMAGE();
    IMAGE(int width, int height);
    IMAGE(int width, int height, color_t color);
    IMAGE(const IMAGE& img);
    IMAGE& operator=(const IMAGE& img);
    ~IMAGE();

    void gentexture(bool gen);

    HDC      getdc() const { return m_hDC; }
    int      getwidth() const { return m_width; }
    int      getheight() const { return m_height; }
    color_t* getbuffer() const { return (color_t*)m_pBuffer; }
#ifdef EGE_GDIPLUS
    // TODO: thread safe?
    Gdiplus::Graphics* getGraphics();
    Gdiplus::Pen*      getPen();
    Gdiplus::Brush*    getBrush();
    void               set_pattern(Gdiplus::Brush* brush);
#endif
    void enable_anti_alias(bool enable);

    int  resize_f(int width, int height);
    int  resize(int width, int height);
    void copyimage(PCIMAGE pSrcImg);

    int getimage(int xSrc, int ySrc, int srcWidth, int srcHeight);
    int getimage(PCIMAGE pSrcImg, int xSrc, int ySrc, int srcWidth, int srcHeight);
    int getimage(const char* imageFile, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(const wchar_t* imageFile, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(const char* resType, const char* resName, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(const wchar_t* resType, const wchar_t* resName, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(void* pMem, long size);

    void putimage(int xDest, int yDest, DWORD dwRop = SRCCOPY) const;
    void putimage(int xDest, int yDest, int widthDest, int heightDest, int xSrc, int ySrc, DWORD dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg, int xDest, int yDest, DWORD dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg,
        int              xDest,
        int              yDest,
        int              widthDest,
        int              heightDest,
        int              xSrc,
        int              ySrc,
        DWORD            dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg,
        int              xDest,
        int              yDest,
        int              widthDest,
        int              heightDest,
        int              xSrc,
        int              ySrc,
        int              srcWidth,
        int              srcHeight,
        DWORD            dwRop = SRCCOPY) const;

    int saveimage(const char*  filename, bool withAlphaChannel = false) const;
    int saveimage(const wchar_t* filename, bool withAlphaChannel = false) const;
    int savepngimg(FILE* fp, bool withAlphaChannel = false) const;

    int getpngimg(FILE* fp);

    int putimage_transparent(PIMAGE imgDest,                // handle to dest
        int                         xDest,                  // x-coord of destination upper-left corner
        int                         yDest,                  // y-coord of destination upper-left corner
        color_t                     transparentColor,       // color to make transparent
        int                         xSrc = 0,               // x-coord of source upper-left corner
        int                         ySrc = 0,               // y-coord of source upper-left corner
        int                         widthSrc   = 0,         // width of source rectangle
        int                         heightSrc  = 0          // height of source rectangle
    ) const;

    int putimage_alphablend(PIMAGE imgDest,                 // handle to dest
        int                        xDest,                   // x-coord of destination upper-left corner
        int                        yDest,                   // y-coord of destination upper-left corner
        unsigned char              alpha,                   // alpha
        int                        xSrc = 0,                // x-coord of source upper-left corner
        int                        ySrc = 0,                // y-coord of source upper-left corner
        int                        widthSrc   = 0,          // width of source rectangle
        int                        heightSrc  = 0,          // height of source rectangle
        color_type                 colorType  = COLORTYPE_PRGB32  // alpha mode(straight alpha or premultiplied alpha)
    ) const;

    int putimage_alphablend(PIMAGE imgDest,                 // handle to dest
        int                        xDest,                   // x-coord of destination upper-left corner
        int                        yDest,                   // y-coord of destination upper-left corner
        int                        widthDest,               // width of source rectangle
        int                        heightDest,              // height of source rectangle
        unsigned char              alpha,                   // alpha
        int                        xSrc,                    // x-coord of source upper-left corner
        int                        ySrc,                    // y-coord of source upper-left corner
        int                        widthSrc,                // width of source rectangle
        int                        heightSrc,               // height of source rectangle
        bool                       smooth = false,
        color_type                 colorType  = COLORTYPE_PRGB32  // alpha mode(straight alpha or premultiplied alpha)
    ) const;

    int putimage_alphatransparent(PIMAGE imgDest,           // handle to dest
        int                              xDest,             // x-coord of destination upper-left corner
        int                              yDest,             // y-coord of destination upper-left corner
        color_t                          transparentColor,  // color to make transparent
        unsigned char                    alpha,             // alpha
        int                              xSrc = 0,          // x-coord of source upper-left corner
        int                              ySrc = 0,          // y-coord of source upper-left corner
        int                              widthSrc   = 0,    // width of source rectangle
        int                              heightSrc  = 0     // height of source rectangle
    ) const;

    int putimage_withalpha(PIMAGE imgDest,          // handle to dest
        int                       xDest,            // x-coord of destination upper-left corner
        int                       yDest,            // y-coord of destination upper-left corner
        int                       xSrc = 0,         // x-coord of source upper-left corner
        int                       ySrc = 0,         // y-coord of source upper-left corner
        int                       widthSrc   = 0,   // width of source rectangle
        int                       heightSrc  = 0    // height of source rectangle
    ) const;

    int putimage_withalpha(PIMAGE imgDest,          // handle to dest
        int                       xDest,            // x-coord of destination upper-left corner
        int                       yDest,            // y-coord of destination upper-left corner
        int                       widthDest,        // width of destination rectangle
        int                       heightDest,       // height of destination rectangle
        int                       xSrc,             // x-coord of source upper-left corner
        int                       ySrc,             // y-coord of source upper-left corner
        int                       widthSrc,         // width of source rectangle
        int                       heightSrc,        // height of source rectangle
        bool                      smooth = false
    ) const;

    int putimage_alphafilter(PIMAGE imgDest,        // handle to dest
        int                         xDest,          // x-coord of destination upper-left corner
        int                         yDest,          // y-coord of destination upper-left corner
        PCIMAGE                     imgAlpha,       // alpha
        int                         xSrc = 0,       // x-coord of source upper-left corner
        int                         ySrc = 0,       // y-coord of source upper-left corner
        int                         widthSrc   = 0, // width of source rectangle
        int                         heightSrc  = 0  // height of source rectangle
    ) const;

    int imagefilter_blurring_4(int intensity,
        int                        alpha,
        int                        xDest,
        int                        yDest,
        int                        widthDest,
        int                        heightDest);

    int imagefilter_blurring_8(int intensity,
        int                        alpha,
        int                        xDest,
        int                        yDest,
        int                        widthDest,
        int                        heightDest);

    int imagefilter_blurring(int intensity,
        int                      alpha,
        int                      xDest = 0,
        int                      yDest = 0,
        int                      widthDest   = 0,
        int                      heightDest  = 0);

    int putimage_rotate(PIMAGE imgtexture,
        int                    xDest,
        int                    yDest,
        float                  centerx,
        float                  centery,
        float                  radian,
        int                    btransparent = 0,  // transparent (1) or not (0)
        int                    alpha        = -1, // in range[0, 256], alpha== -1 means no alpha
        int                    smooth       = 0);

    int putimage_rotatezoom(PIMAGE imgtexture,
        int                        xDest,
        int                        yDest,
        float                      centerx,
        float                      centery,
        float                      radian,
        float                      zoom,
        int                        btransparent = 0,  // transparent (1) or not (0)
        int                        alpha        = -1, // in range[0, 256], alpha== -1 means no alpha
        int                        smooth       = 0);

    friend graphics_errors getimage_from_png_struct(PIMAGE, void*, void*);
};

graphics_errors getimage_from_bitmap(PIMAGE pimg, Gdiplus::Bitmap& bitmap);

int savebmp(PCIMAGE pimg, FILE* file, bool alpha = false);

int image_premultiply(PIMAGE pimg);

int image_premultiply(color_t* pixels, int width, int height);

int image_premultiply(color_t* dst, const color_t* src, int width, int height);

// 图像做预乘 alpha 处理，像素颜色由 (a, R, G, B) 转换为 (a, a*R, a*G, a*B) (这里 a 归一化为 0.0~1.0)
// dstStride 和 srcStride 是以像素为单位
int image_premultiply(color_t* dst, const color_t* src, int width, int height, int dstStride, int srcStride);

int image_unpremultiply(PIMAGE pimg, bool opaque = false);

int image_unpremultiply(color_t* pixels, int width, int height, bool opaque = false);

int image_unpremultiply(color_t* dst, const color_t* src, int width, int height, bool opaque = false);

// 图像的 RGB 去预乘 alpha 处理，像素颜色由 (a, a*R, a*G, a*B) 转换为 (a, R, G, B) (这里 a 归一化为 0.0~1.0)
// dstStride 和 srcStride 是以像素为单位
// 如果 opaque 为 true，RGB 通道去预乘 alpha 后还会将 alpha通道设置为 0xFF
int image_unpremultiply(color_t* dst, const color_t* src, int width, int height, int dstStride, int srcStride, bool opaque = false);

ImageFormat checkImageFormatByFileName(const wchar_t* fileName);

ImageDecodeFormat getImageDecodeFormat(ImageFormat imageformat);

ImageEncodeFormat getImageEncodeFormat(ImageFormat imageformat);
} // namespace ege
