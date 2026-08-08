#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_NON_CONFORMING_SWPRINTFS
#endif

#include "ege_head.h"
#include "ege_common.h"
//#include "ege_extension.h"
#include "gdi_conv.h"

//#include <cmath>
//#include <cstdarg>
//#include <cstdio>

namespace ege
{

ege_path::ege_path()
{
    gdiplusinit();
    m_data = new Gdiplus::GraphicsPath;
}

ege_path::ege_path(const ege_point *points, const unsigned char *types, int count)
{
    gdiplusinit();
    m_data = new Gdiplus::GraphicsPath((const Gdiplus::PointF*)points, (const BYTE*)types, count);
}

ege_path::ege_path(const ege_path &path)
{
    const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path.m_data;
    m_data = (graphicsPath != NULL) ? graphicsPath->Clone() : NULL;
}

ege_path::~ege_path()
{
    if (m_data != NULL) {
        delete (Gdiplus::GraphicsPath*)m_data;
    }
}

const void* ege_path::data() const
{
    return m_data;
}

void* ege_path::data()
{
    return m_data;
}

ege_path& ege_path::operator=(const ege_path& path)
{
    if (this != &path) {
        if (m_data != NULL) {
            delete (Gdiplus::GraphicsPath*)m_data;
        }

        const Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path.m_data;
        m_data = (graphicsPath != NULL) ? graphicsPath->Clone() : NULL;
    }

    return *this;
}

void ege_drawpath(const ege_path* path, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if ((img != NULL) && (path != NULL)) {
        Gdiplus::Graphics* graphics = img->getGraphics();
        graphics->DrawPath(img->getPen(), (Gdiplus::GraphicsPath*)path->data());
    }
    CONVERT_IMAGE_END;
}

void ege_fillpath(const ege_path* path, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if ((img != NULL) && (path != NULL)) {
        const Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::Graphics* graphics = img->getGraphics();
            graphics->FillPath(img->getBrush(), graphicsPath);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_drawpath(const ege_path* path, float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if ((img != NULL) && (path != NULL)) {
        const Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::Graphics* graphics = img->getGraphics();
            graphics->TranslateTransform(x, y);
            graphics->DrawPath(img->getPen(), graphicsPath);
            graphics->TranslateTransform(-x, -y);
        }
    }
    CONVERT_IMAGE_END;
}

void ege_fillpath(const ege_path* path, float x, float y, PIMAGE pimg)
{
    PIMAGE img = CONVERT_IMAGE(pimg);
    if ((img != NULL) && (path != NULL)) {
        const Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::Graphics* graphics = img->getGraphics();
            graphics->TranslateTransform(x, y);
            graphics->FillPath(img->getBrush(), graphicsPath);
            graphics->TranslateTransform(-x, -y);
        }
    }
    CONVERT_IMAGE_END;
}

ege_path* ege_path_create()
{
    return new(std::nothrow) ege_path;
}

ege_path* ege_path_createfrom(const ege_point* points, const unsigned char* types, int count)
{
    return new(std::nothrow) ege_path(points, types, count);
}

ege_path* ege_path_clone(const ege_path* path)
{
    if (path == NULL)
        return NULL;

    return new(std::nothrow) ege_path(*path);
}

void ege_path_destroy(const ege_path* path)
{
    delete path;
}

void ege_path_start(ege_path* path)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->StartFigure();
        }
    }
}

void ege_path_close(ege_path* path)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->CloseFigure();
        }
    }
}

void ege_path_closeall(ege_path* path)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->CloseAllFigures();
        }
    }
}

void ege_path_setfillmode(ege_path* path, fill_mode mode)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::FillMode fillMode = Gdiplus::FillModeAlternate;
            switch (mode) {
                case FILLMODE_ALTERNATE: fillMode = Gdiplus::FillModeAlternate; break;
                case FILLMODE_WINDING:   fillMode = Gdiplus::FillModeWinding;   break;
                default:                                                        break;
            }
            graphicsPath->SetFillMode(fillMode);
        }
    }
}

void ege_path_reset(ege_path* path)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->Reset();
        }
    }
}

void ege_path_reverse(ege_path* path)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->Reverse();
        }
    }
}

void ege_path_widen(ege_path* path, float lineWidth, const ege_transform_matrix* matrix)
{
    ege_path_widen(path, lineWidth, matrix, Gdiplus::FlatnessDefault);
}

void ege_path_widen(ege_path* path, float lineWidth, const ege_transform_matrix* matrix,  float flatness)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            const Gdiplus::Pen pen(Gdiplus::Color(), lineWidth);

            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->Widen(&pen, &mat, flatness);
            } else {
                graphicsPath->Widen(&pen, NULL, flatness);
            }
        }
    }
}

void ege_path_flatten(ege_path* path, const ege_transform_matrix* matrix)
{
    ege_path_flatten(path, matrix, Gdiplus::FlatnessDefault);
}

void ege_path_flatten(ege_path* path, const ege_transform_matrix* matrix, float flatness)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();

        if (graphicsPath != NULL) {
            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->Flatten(&mat, flatness);
            } else {
                graphicsPath->Flatten(NULL, flatness);
            }
        }
    }
}

void ege_path_warp(ege_path* path, const ege_point* points, int count, const ege_rect* rect,
     const ege_transform_matrix* matrix)
{
    ege_path_warp(path, points, count , rect, matrix, Gdiplus::FlatnessDefault);
}

void ege_path_warp(ege_path* path, const ege_point* points, int count, const ege_rect* rect,
    const ege_transform_matrix* matrix, float flatness)
{
    if ((path != NULL) && (points != NULL) && (rect != NULL) && ((count == 3) || (count == 4))) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            const Gdiplus::PointF* p = (const Gdiplus::PointF*)points;
            const Gdiplus::RectF r(rect->x, rect->y, rect->w, rect->h);

            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->Warp(p, count, r, &mat, Gdiplus::WarpModePerspective, flatness);
            } else {
                graphicsPath->Warp(p, count, r, NULL, Gdiplus::WarpModePerspective, flatness);
            }
        }
    }
}

void ege_path_outline(ege_path* path, const ege_transform_matrix* matrix)
{
    ege_path_outline(path, matrix, Gdiplus::FlatnessDefault);
}

void ege_path_outline(ege_path* path, const ege_transform_matrix* matrix, float flatness)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->Outline(&mat, flatness);
            } else {
                graphicsPath->Outline(NULL, flatness);
            }
        }
    }
}

bool ege_path_inpath(const ege_path* path, float x, float y)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            return graphicsPath->IsVisible(x, y);
        }
    }
    return false;
}

bool ege_path_inpath(const ege_path* path, float x, float y, PCIMAGE pimg)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            PIMAGE img = CONVERT_IMAGE_CONST((PIMAGE)pimg);
            if ((img != NULL) && (img->m_hDC != NULL)) {
                return graphicsPath->IsVisible(x, y, img->getGraphics());
            }
        }
    }
    return false;
}

bool ege_path_instroke(const ege_path* path, float x, float y)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::Pen pen(Gdiplus::Color(), 1.0f);
            return graphicsPath->IsOutlineVisible(x, y, &pen);
        }
    }
    return false;
}

bool ege_path_instroke(const ege_path* path, float x, float y, PCIMAGE pimg)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            PIMAGE img = CONVERT_IMAGE_CONST((PIMAGE)pimg);
            if ((img != NULL) && (img->m_hDC != NULL)) {
                return graphicsPath->IsOutlineVisible(x, y, img->getPen(), img->getGraphics());
            }
        }
    }
    return false;
}

ege_point ege_path_lastpoint(const ege_path* path)
{
    ege_point lastPoint = {0.0f, 0.0f};
    if (path != NULL) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->GetLastPoint((Gdiplus::PointF*)&lastPoint);
        }
    }
    return lastPoint;
}

int ege_path_pointcount(const ege_path* path)
{
    int pointCount = 0;
    if (path != NULL) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            pointCount = graphicsPath->GetPointCount();
        }
    }
    return pointCount;
}

ege_rect ege_path_getbounds(const ege_path* path, const ege_transform_matrix* matrix)
{
    ege_rect bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    if (path != NULL) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->GetBounds((Gdiplus::RectF*)&bounds, &mat);
            } else {
                graphicsPath->GetBounds((Gdiplus::RectF*)&bounds, NULL);
            }
        }
    }

    return bounds;
}

ege_rect ege_path_getbounds(const ege_path* path, const ege_transform_matrix* matrix, PCIMAGE pimg)
{
    ege_rect bounds = {0.0f, 0.0f, 0.0f, 0.0f};
    if (path != NULL) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            PIMAGE img = CONVERT_IMAGE_CONST((PIMAGE)pimg);
            if (matrix != NULL) {
                Gdiplus::Matrix mat;
                matrixConvert(*matrix, mat);
                graphicsPath->GetBounds((Gdiplus::RectF*)&bounds, &mat, img->getPen());
            } else {
                graphicsPath->GetBounds((Gdiplus::RectF*)&bounds, NULL, img->getPen());
            }
            CONVERT_IMAGE_END
        }
    }

    return bounds;
}

ege_point* ege_path_getpathpoints(const ege_path* path, ege_point* points)
{
    if ((path != NULL)) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            int pointCount = graphicsPath->GetPointCount();

            if (points == NULL) {
                points = new(std::nothrow) ege_point[pointCount];
            }

            if (points != NULL) {
                graphicsPath->GetPathPoints((Gdiplus::PointF*)points, pointCount);
            }
            return points;
        }
    }

    return NULL;
}

unsigned char* ege_path_getpathtypes(const ege_path* path, unsigned char* types)
{
    if ((path != NULL)) {
        const Gdiplus::GraphicsPath* graphicsPath = (const Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            int pointCount = graphicsPath->GetPointCount();

            if (types == NULL) {
                types = new(std::nothrow) unsigned char[pointCount];
            }

            if (types != NULL) {
                graphicsPath->GetPathTypes(types, pointCount);
            }

            return types;
        }
    }

    return NULL;
}

void ege_path_transform(ege_path* path, const ege_transform_matrix *matrix)
{
    if ((path != NULL) && (matrix != NULL)) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::Matrix mat;
            matrixConvert(*matrix, mat);
            graphicsPath->Transform(&mat);
        }
    }
}

void ege_path_addpath(ege_path* dstPath, const ege_path* srcPath, bool connect)
{
    if ((dstPath != NULL) && (srcPath != NULL)) {
        Gdiplus::GraphicsPath* dstGraphicsPath = (Gdiplus::GraphicsPath*)dstPath->data();
        const Gdiplus::GraphicsPath* srcGraphicsPath = (const Gdiplus::GraphicsPath*)srcPath->data();
        if ((dstGraphicsPath != NULL) && (srcGraphicsPath != NULL)) {
            dstGraphicsPath->AddPath(srcGraphicsPath, connect);
        }
    }
}

void ege_path_addline(ege_path* path, float x1, float y1, float x2, float y2)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddLine(x1, y1, x2, y2);
        }
    }
}

void ege_path_addarc(ege_path* path, float x, float y, float width, float height, float startAngle, float sweepAngle)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddArc(x, y, width, height, startAngle, sweepAngle);
        }
    }
}

void ege_path_addpolyline(ege_path* path, int numOfPoints, const ege_point *points)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddLines((const Gdiplus::PointF*)points, numOfPoints);
        }
    }
}

void ege_path_addbezier(ege_path* path, int numOfPoints, const ege_point *points)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddBeziers((const Gdiplus::PointF*)points, numOfPoints);
        }
    }
}

void ege_path_addbezier(ege_path* path, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddBezier(x1, y1, x2, y2, x3, y3, x4, y4);
        }
    }
}

void ege_path_addcurve(ege_path* path, int numOfPoints, const ege_point *points)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddCurve((const Gdiplus::PointF*)points, numOfPoints);
        }
    }
}

void ege_path_addcurve(ege_path* path, int numOfPoints, const ege_point *points, float tension)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddCurve((const Gdiplus::PointF*)points, numOfPoints, tension);
        }
    }
}

void ege_path_addcircle(ege_path* path, float x, float y, float radius)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        }
    }
}

void ege_path_addrect(ege_path* path, float x, float y, float width, float height)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            Gdiplus::RectF rect(x, y, width, height);
            graphicsPath->AddRectangle(rect);
        }
    }
}

void ege_path_addellipse(ege_path* path, float x, float y, float width, float height)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddEllipse(x, y, width, height);
        }
    }
}

void ege_path_addpie(ege_path* path, float x, float y, float width, float height, float startAngle, float sweepAngle)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddPie(x, y, width, height, startAngle, sweepAngle);
        }
    }
}

void ege_path_addtext(ege_path* path, float x, float y, const char* text, float height, int length,
     const char* typeface, int fontStyle)
{
    ege_path_addtext(path, x, y, mb2w(text).c_str(), height, length, mb2w(typeface).c_str(), fontStyle);
}

void ege_path_addtext(ege_path* path, float x, float y, const wchar_t* text, float height, int length,
     const wchar_t* typeface, int fontStyle)
{
    if ((path != NULL) && (text != NULL) && (length != 0))  {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {

            Gdiplus::REAL emSize = height;
            Gdiplus::PointF origin(x, y);
            const Gdiplus::StringFormat* format = Gdiplus::StringFormat::GenericTypographic();

            if ((typeface == NULL) || (typeface[0] == L'\0')) {
                typeface = L"SimSun";
            }

            Gdiplus::FontFamily fontFamliy(typeface);

            INT style = 0;
            if (fontStyle & FONTSTYLE_BOLD)       style |= Gdiplus::FontStyleBold;
            if (fontStyle & FONTSTYLE_ITALIC)     style |= Gdiplus::FontStyleItalic;
            if (fontStyle & FONTSTYLE_UNDERLINE)  style |= Gdiplus::FontStyleUnderline;
            if (fontStyle & FONTSTYLE_STRIKEOUT)  style |= Gdiplus::FontStyleStrikeout;

            graphicsPath->AddString(text, length, &fontFamliy, style, emSize, origin, format);
        }
    }
}

void ege_path_addpolygon(ege_path* path, int numOfPoints, const ege_point *points)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddPolygon((const Gdiplus::PointF*)points, numOfPoints);
        }
    }
}

void ege_path_addclosedcurve(ege_path* path, int numOfPoints, const ege_point *points)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddClosedCurve((const Gdiplus::PointF*)points, numOfPoints);
        }
    }
}

void ege_path_addclosedcurve(ege_path* path, int numOfPoints, const ege_point *points, float tension)
{
    if (path != NULL) {
        Gdiplus::GraphicsPath* graphicsPath = (Gdiplus::GraphicsPath*)path->data();
        if (graphicsPath != NULL) {
            graphicsPath->AddClosedCurve((const Gdiplus::PointF*)points, numOfPoints, tension);
        }
    }
}

} // namespace ege
