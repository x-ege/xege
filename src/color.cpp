/*
* EGE (Easy Graphics Engine)
* filename  color.cpp

色彩模型转换函数定义
*/

#include "ege_head.h"
#include "ege_common.h"
#include <math.h>

namespace ege
{

/* private function */
static COLORHSL EGE_PRIVATE_RGBtoHSL(int _col)
{
    COLORHSL _crCol;
    float    r, g, b;
    float *  t, *dp[3] = {&r, &g, &b};

    b = EGEGET_B(_col) / 255.0f;
    g = EGEGET_G(_col) / 255.0f;
    r = EGEGET_R(_col) / 255.0f;

    {
        auto ifswap = [](float* a, float* b) {
            if (*a > *b) {
                float t = *a;
                *a      = *b;
                *b      = t;
            }
        };
        ifswap(dp[0], dp[1]);
        ifswap(dp[1], dp[2]);
        ifswap(dp[0], dp[1]);
    }

    {
        _crCol.l = (*dp[0] + *dp[2]) / 2;
        if (_crCol.l < 1e-2f) {
            _crCol.l = 0;
            _crCol.h = 0;
            _crCol.s = 0;
            return _crCol;
        }
        if (_crCol.l > 0.99) {
            _crCol.l = 1;
            _crCol.h = 0;
            _crCol.s = 0;
            return _crCol;
        }
        if (fabs(_crCol.l - 0.5) < 1e-2) {
            _crCol.l = 0.5;
        }
    }

    if (_crCol.l == 0.5) {
        ;
    } else if (_crCol.l < 0.5) {
        auto blackunformat = [](float c, float v) -> float { return c / ((v) * 2); };
        for (int n = 0; n < 3; ++n) {
            *dp[n] = blackunformat(*dp[n], _crCol.l);
            if (*dp[n] > 1) {
                *dp[n] = 1;
            }
        }
    } else {
        auto whiteunformat = [](float c, float v) -> float { return 1 - (1 - c) / ((1 - (v)) * 2); };
        for (int n = 0; n < 3; ++n) {
            *dp[n] = whiteunformat(*dp[n], _crCol.l);
            if (*dp[n] > 1) {
                *dp[n] = 1;
            }
        }
    }

    {
        _crCol.s = *dp[2] * 2 - 1;
        if (_crCol.s < 1e-2) {
            _crCol.h = 0;
            return _crCol;
        }
    }

    {
        auto satunformat = [](float c, float s) -> float { return (c - 0.5f) / s + 0.5f; };
        for (int n = 0; n < 3; ++n) {
            *dp[n] = satunformat(*dp[n], _crCol.s);
            if (*dp[n] > 1) {
                *dp[n] = 1;
            }
        }
    }

    {
        _crCol.h = *dp[1];
        if ((dp[2] == &r) && (dp[1] == &g)) {
            _crCol.h = _crCol.h + 0;
        } else if ((dp[2] == &g) && (dp[1] == &b)) {
            _crCol.h = _crCol.h + 2;
        } else if ((dp[2] == &b) && (dp[1] == &r)) {
            _crCol.h = _crCol.h + 4;
        } else if ((dp[2] == &g) && (dp[1] == &r)) {
            _crCol.h = 2 - _crCol.h;
        } else if ((dp[2] == &b) && (dp[1] == &g)) {
            _crCol.h = 4 - _crCol.h;
        } else if ((dp[2] == &r) && (dp[1] == &b)) {
            _crCol.h = 6 - _crCol.h;
        }
        _crCol.h /= 6;
    }
    return _crCol;
}

/* private function */
static int EGE_PRIVATE_HSLtoRGB(float _h, float _s, float _l)
{
    float r, g, b;

    if (_h < 0.0f) {
        _h += (float)(int)(-_h + 1);
    }
    if (_h >= 1.0f) {
        _h -= (float)(int)(_h);
    }
    if (_s == 0) {
        r = _l;
        g = _l;
        b = _l;
    } else {
        float dp[3];
        float xh = _h * 6;
        if (xh == 6) {
            xh = 0;
        }
        int i  = (int)(floor(xh) + 0.1), n;
        dp[0]  = 1;
        dp[2]  = 0;
        xh    -= i;
        if (i & 1) {
            dp[1] = 1 - xh;
        } else {
            dp[1] = xh;
        }

        auto satformat = [](float c, float s) { return (c - 0.5f) * s + 0.5f; };
        for (n = 0; n < 3; ++n) {
            dp[n] = satformat(dp[n], _s);
        }

        if (_l == 0.5f) {
            ;
        } else if (_l < 0.5f) {
            auto blackformat = [](float c, float v) { return c * v * 2; };
            if (_l < 0) {
                _l = 0;
            }
            for (n = 0; n < 3; ++n) {
                dp[n] = blackformat(dp[n], _l);
            }
        } else {
            auto whiteformat = [](float c, float v) { return 1 - (1 - c) * (1 - v) * 2; };
            if (_l > 1) {
                _l = 1;
            }
            for (n = 0; n < 3; ++n) {
                dp[n] = whiteformat(dp[n], _l);
            }
        }

        {
            if (i == 0) {
                r = dp[0];
                g = dp[1];
                b = dp[2];
            } else if (i == 1) {
                r = dp[1];
                g = dp[0];
                b = dp[2];
            } else if (i == 2) {
                r = dp[2];
                g = dp[0];
                b = dp[1];
            } else if (i == 3) {
                r = dp[2];
                g = dp[1];
                b = dp[0];
            } else if (i == 4) {
                r = dp[1];
                g = dp[2];
                b = dp[0];
            } else {
                r = dp[0];
                g = dp[2];
                b = dp[1];
            }
        }
    }

    return EGERGB(DWORD(round(r * 255)), DWORD(round(g * 255)), DWORD(round(b * 255)));
}

/* private function */
static void RGB_TO_HSV(const COLORRGB* input, COLORHSV* output)
{
    float r, g, b, minRGB, maxRGB, deltaRGB;
    r = input->r / 255.0f;
    g = input->g / 255.0f;
    b = input->b / 255.0f;

    minRGB    = min(r, g, b);
    maxRGB    = max(r, g, b);
    deltaRGB  = maxRGB - minRGB;
    output->v = maxRGB;

    if (maxRGB != 0.0f) {
        output->s = deltaRGB / maxRGB;
    } else {
        output->s = 0.0f;
    }
    if (output->s <= 0.0f) {
        output->h = -1.0f;
    } else {
        if (r == maxRGB) {
            output->h = (g - b) / deltaRGB;
        } else {
            if (g == maxRGB) {
                output->h = 2.0f + (b - r) / deltaRGB;
            } else {
                if (b == maxRGB) {
                    output->h = 4.0f + (r - g) / deltaRGB;
                }
            }
        }
        output->h = output->h * 60.0f;
        if (output->h < 0.0f) {
            output->h += 360.0f;
        }
        output->h /= 360.0f;
    }
}

/* private function */
static void HSV_TO_RGB(COLORHSV* input, COLORRGB* output)
{
    float R = 0, G = 0, B = 0;
    int   k;
    float aa, bb, cc, f;
    if (input->s <= 0.0) {
        R = G = B = input->v;
    } else {
        if (input->h == 1.0) {
            input->h = 0.0;
        }
        input->h *= 6.0;

        k  = (int)floor(input->h); // ??
        f  = input->h - k;
        aa = input->v * (1.0f - input->s);
        bb = input->v * (1.0f - input->s * f);
        cc = input->v * (1.0f - (input->s * (1.0f - f)));

        switch (k) {
        case 0:
            R = input->v;
            G = cc;
            B = aa;
            break;
        case 1:
            R = bb;
            G = input->v;
            B = aa;
            break;
        case 2:
            R = aa;
            G = input->v;
            B = cc;
            break;
        case 3:
            R = aa;
            G = bb;
            B = input->v;
            break;
        case 4:
            R = cc;
            G = aa;
            B = input->v;
            break;
        case 5:
            R = input->v;
            G = aa;
            B = bb;
            break;
        }
    }

    output->r = (unsigned char)round(R * 255);
    output->g = (unsigned char)round(G * 255);
    output->b = (unsigned char)round(B * 255);
}

color_t rgb2gray(color_t color)
{
    double  c;
    color_t r;
    c  = ((color >> 16) & 0xFF) * 0.299;
    c += ((color >> 8) & 0xFF) * 0.587;
    c += ((color) & 0xFF) * 0.114;
    r  = (color_t)round(c);
    return EGERGB(r, r, r);
}

void rgb2hsl(color_t rgb, float* H, float* S, float* L)
{
    COLORHSL hsl = EGE_PRIVATE_RGBtoHSL((int)rgb);

    *H = hsl.h * 360.0f;
    *S = hsl.s;
    *L = hsl.l;
}

color_t hsl2rgb(float H, float S, float L)
{
    return (color_t)EGE_PRIVATE_HSLtoRGB(H / 360.0f, S, L);
}

void rgb2hsv(color_t rgb, float* H, float* S, float* V)
{
    COLORRGB crgb;
    COLORHSV chsv;
    crgb.r = (unsigned char)EGEGET_R(rgb);
    crgb.g = (unsigned char)EGEGET_G(rgb);
    crgb.b = (unsigned char)EGEGET_B(rgb);
    RGB_TO_HSV(&crgb, &chsv);
    *H = chsv.h * 360.0f;
    *S = chsv.s;
    *V = chsv.v;
}

color_t hsv2rgb(float H, float S, float V)
{
    COLORRGB crgb;
    if (H < 0.0f) {
        H += (float)(int)(-H / 360.0f + 1) * 360.0f;
    }
    if (H >= 360.0f) {
        H -= (float)(int)(H / 360.0f) * 360.0f;
    }
    COLORHSV chsv = {H / 360.0f, S, V};
    HSV_TO_RGB(&chsv, &crgb);

    return EGERGB(crgb.r, crgb.g, crgb.b);
}

color_t colorblend(color_t dst, color_t src, unsigned char alpha)
{
    return colorblend_inline(dst, src, alpha);
}

color_t colorblend_f(color_t dst, color_t src, unsigned char alpha)
{
    return colorblend_inline_fast(dst, src, alpha);
}

color_t alphablend(color_t dst, color_t src)
{
    return alphablend_inline(dst, src);
}

color_t alphablend(color_t dst, color_t src, unsigned char srcAlphaFactor)
{
    return alphablend_inline(dst, src, srcAlphaFactor);
}

color_t alphablend_premultiplied(color_t dst, color_t src)
{
    return alphablend_premultiplied_inline(dst, src);
}

color_t alphablend_premultiplied(color_t dst, color_t src, unsigned char srcAlphaFactor)
{
    byte alpha = DIVIDE_255_FAST(EGEGET_A(src) * srcAlphaFactor + 255 / 2);
    byte red   = DIVIDE_255_FAST(EGEGET_R(src) * srcAlphaFactor + 255 / 2);
    byte green = DIVIDE_255_FAST(EGEGET_G(src) * srcAlphaFactor + 255 / 2);
    byte blue  = DIVIDE_255_FAST(EGEGET_B(src) * srcAlphaFactor + 255 / 2);

    return alphablend_premultiplied_inline(dst, EGEARGB(alpha, red, green, blue));
}

} // namespace ege
