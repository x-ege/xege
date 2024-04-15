#pragma once

#include "ege_head.h"
#include <Windows.h>


namespace ege
{

// 定义图像对象
class IMAGE
{
    int m_initflag;

public:
    HDC     m_hDC;
    HBITMAP m_hBmp;
    int     m_width;
    int     m_height;
    PDWORD  m_pBuffer;
    color_t m_color;
    color_t m_fillcolor;

private:
#ifdef EGE_GDIPLUS
    Gdiplus::Graphics* m_graphics;
    Gdiplus::Pen*      m_pen;
    Gdiplus::Brush*    m_brush;
#endif
    bool m_aa;
    void initimage(HDC refDC, int width, int height);
    void construct(int width, int height);
    void setdefaultattribute();
    int  deleteimage();
    void reset();

public:
    viewporttype     m_vpt;
    textsettingstype m_texttype;
    linestyletype    m_linestyle;
    float            m_linewidth;
    color_t          m_bk_color;
    void*            m_texture;

private:
    void inittest(const WCHAR* strCallFunction = NULL) const;

public:
    IMAGE();
    IMAGE(int width, int height);
    IMAGE(const IMAGE& img);            // 拷贝构造函数
    IMAGE& operator=(const IMAGE& img); // 赋值运算符重载函数
    ~IMAGE();
    void gentexture(bool gen);

public:
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

    int getimage(int srcX, int srcY, int srcWidth, int srcHeight);
    int getimage(PCIMAGE pSrcImg, int srcX, int srcY, int srcWidth, int srcHeight);
    int getimage(LPCSTR pImgFile, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(LPCWSTR pImgFile, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(LPCSTR pResType, LPCSTR pResName, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(LPCWSTR pResType, LPCWSTR pResName, int zoomWidth = 0, int zoomHeight = 0);
    int getimage(void* pMem, long size);

    void putimage(int dstX, int dstY, DWORD dwRop = SRCCOPY) const;
    void putimage(int dstX, int dstY, int dstWidth, int dstHeight, int srcX, int srcY, DWORD dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg, int dstX, int dstY, DWORD dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg,
        int              dstX,
        int              dstY,
        int              dstWidth,
        int              dstHeight,
        int              srcX,
        int              srcY,
        DWORD            dwRop = SRCCOPY) const;
    void putimage(PIMAGE pDstImg,
        int              dstX,
        int              dstY,
        int              dstWidth,
        int              dstHeight,
        int              srcX,
        int              srcY,
        int              srcWidth,
        int              srcHeight,
        DWORD            dwRop = SRCCOPY) const;

    int saveimage(LPCSTR filename) const;
    int saveimage(LPCWSTR filename) const;
    int savepngimg(FILE* fp, int bAlpha) const;

    int getpngimg(FILE* fp);

    int putimage_transparent(PIMAGE imgdest,         // handle to dest
        int                         nXOriginDest,    // x-coord of destination upper-left corner
        int                         nYOriginDest,    // y-coord of destination upper-left corner
        color_t                     crTransparent,   // color to make transparent
        int                         nXOriginSrc = 0, // x-coord of source upper-left corner
        int                         nYOriginSrc = 0, // y-coord of source upper-left corner
        int                         nWidthSrc   = 0, // width of source rectangle
        int                         nHeightSrc  = 0  // height of source rectangle
    ) const;

    int putimage_alphablend(PIMAGE imgdest,         // handle to dest
        int                        nXOriginDest,    // x-coord of destination upper-left corner
        int                        nYOriginDest,    // y-coord of destination upper-left corner
        unsigned char              alpha,           // alpha
        int                        nXOriginSrc = 0, // x-coord of source upper-left corner
        int                        nYOriginSrc = 0, // y-coord of source upper-left corner
        int                        nWidthSrc   = 0, // width of source rectangle
        int                        nHeightSrc  = 0  // height of source rectangle
    ) const;

    int putimage_alphatransparent(PIMAGE imgdest,         // handle to dest
        int                              nXOriginDest,    // x-coord of destination upper-left corner
        int                              nYOriginDest,    // y-coord of destination upper-left corner
        color_t                          crTransparent,   // color to make transparent
        unsigned char                    alpha,           // alpha
        int                              nXOriginSrc = 0, // x-coord of source upper-left corner
        int                              nYOriginSrc = 0, // y-coord of source upper-left corner
        int                              nWidthSrc   = 0, // width of source rectangle
        int                              nHeightSrc  = 0  // height of source rectangle
    ) const;

    int putimage_withalpha(PIMAGE imgdest,         // handle to dest
        int                       nXOriginDest,    // x-coord of destination upper-left corner
        int                       nYOriginDest,    // y-coord of destination upper-left corner
        int                       nXOriginSrc = 0, // x-coord of source upper-left corner
        int                       nYOriginSrc = 0, // y-coord of source upper-left corner
        int                       nWidthSrc   = 0, // width of source rectangle
        int                       nHeightSrc  = 0  // height of source rectangle
    ) const;

    int putimage_withalpha(PIMAGE imgdest,      // handle to dest
        int                       nXOriginDest, // x-coord of destination upper-left corner
        int                       nYOriginDest, // y-coord of destination upper-left corner
        int                       nWidthDest,   // width of destination rectangle
        int                       nHeightDest,  // height of destination rectangle
        int                       nXOriginSrc,  // x-coord of source upper-left corner
        int                       nYOriginSrc,  // y-coord of source upper-left corner
        int                       nWidthSrc,    // width of source rectangle
        int                       nHeightSrc    // height of source rectangle
    ) const;

    int putimage_alphafilter(PIMAGE imgdest,         // handle to dest
        int                         nXOriginDest,    // x-coord of destination upper-left corner
        int                         nYOriginDest,    // y-coord of destination upper-left corner
        PCIMAGE                     imgalpha,        // alpha
        int                         nXOriginSrc = 0, // x-coord of source upper-left corner
        int                         nYOriginSrc = 0, // y-coord of source upper-left corner
        int                         nWidthSrc   = 0, // width of source rectangle
        int                         nHeightSrc  = 0  // height of source rectangle
    ) const;

    int imagefilter_blurring_4(int intensity,
        int                        alpha,
        int                        nXOriginDest,
        int                        nYOriginDest,
        int                        nWidthDest,
        int                        nHeightDest);

    int imagefilter_blurring_8(int intensity,
        int                        alpha,
        int                        nXOriginDest,
        int                        nYOriginDest,
        int                        nWidthDest,
        int                        nHeightDest);

    int imagefilter_blurring(int intensity,
        int                      alpha,
        int                      nXOriginDest = 0,
        int                      nYOriginDest = 0,
        int                      nWidthDest   = 0,
        int                      nHeightDest  = 0);

    int putimage_rotate(PIMAGE imgtexture,
        int                    nXOriginDest,
        int                    nYOriginDest,
        float                  centerx,
        float                  centery,
        float                  radian,
        int                    btransparent = 0,  // transparent (1) or not (0)
        int                    alpha        = -1, // in range[0, 256], alpha== -1 means no alpha
        int                    smooth       = 0);

    int putimage_rotatezoom(PIMAGE imgtexture,
        int                        nXOriginDest,
        int                        nYOriginDest,
        float                      centerx,
        float                      centery,
        float                      radian,
        float                      zoom,
        int                        btransparent = 0,  // transparent (1) or not (0)
        int                        alpha        = -1, // in range[0, 256], alpha== -1 means no alpha
        int                        smooth       = 0);

    friend void getimage_from_png_struct(PIMAGE, void*, void*);
};

int getimage_from_bitmap(PIMAGE pimg, Gdiplus::Bitmap& bitmap);

} // namespace ege
