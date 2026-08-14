#include "test_support.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace ege {
namespace test {
namespace {

constexpr int kWidth = 180;
constexpr int kHeight = 140;
const color_t kBackground = EGERGB(3, 7, 13);
const color_t kRed = EGERGB(231, 76, 60);
const color_t kGreen = EGERGB(46, 204, 113);
const color_t kBlue = EGERGB(52, 152, 219);
const color_t kYellow = EGERGB(241, 196, 15);

using PathPtr = std::unique_ptr<ege_path, void (*)(const ege_path*)>;

PathPtr makePath()
{
    return PathPtr(ege_path_create(), ege_path_destroy);
}

bool near(float actual, float expected, float tolerance = 0.01f)
{
    return std::abs(actual - expected) <= tolerance;
}

int countNotColor(PCIMAGE image, color_t color)
{
    const color_t* pixels = getbuffer(image);
    const int count = getwidth(image) * getheight(image);
    int changed = 0;
    for (int index = 0; index < count; ++index) {
        changed += pixels[index] != color;
    }
    return changed;
}

void clearImage(PIMAGE image)
{
    setbkcolor(kBackground, image);
    cleardevice(image);
    setviewport(0, 0, getwidth(image), getheight(image), 1, image);
    ege_transform_reset(image);
    ege_setpattern_none(image);
}

void testEnhancedPrimitivesAndAlpha()
{
    Image image(kWidth, kHeight);
    EGE_CHECK(image.value != nullptr);
    clearImage(image.value);
    setlinecolor(kYellow, image.value);
    setfillcolor(kGreen, image.value);
    setlinestyle(PS_SOLID, 0, 1, image.value);

    const ege_point open[] = {
        {4.0f, 16.0f}, {16.0f, 4.0f}, {28.0f, 16.0f}, {40.0f, 4.0f}
    };
    const ege_point closed[] = {
        {8.0f, 42.0f}, {22.0f, 28.0f}, {36.0f, 42.0f}, {22.0f, 54.0f}
    };

    ege_enable_aa(false, image.value);
    ege_line(2.0f, 2.0f, 45.0f, 2.0f, image.value);
    ege_drawpoly(4, open, image.value);
    ege_polyline(4, open, image.value);
    ege_polygon(4, closed, image.value);
    ege_fillpoly(4, closed, image.value);
    ege_bezier(4, open, image.value);
    ege_drawbezier(4, open, image.value);
    ege_drawcurve(4, open, image.value);
    ege_drawcurve(4, open, 0.35f, image.value);
    ege_drawclosedcurve(4, closed, image.value);
    ege_drawclosedcurve(4, closed, 0.35f, image.value);
    ege_fillclosedcurve(4, closed, image.value);
    ege_fillclosedcurve(4, closed, 0.35f, image.value);

    ege_rectangle(50.0f, 4.0f, 24.0f, 18.0f, image.value);
    ege_fillrect(78.0f, 4.0f, 24.0f, 18.0f, image.value);
    ege_circle(116.0f, 13.0f, 9.0f, image.value);
    ege_fillcircle(140.0f, 13.0f, 9.0f, image.value);
    ege_ellipse(50.0f, 28.0f, 24.0f, 18.0f, image.value);
    ege_fillellipse(78.0f, 28.0f, 24.0f, 18.0f, image.value);
    ege_arc(108.0f, 28.0f, 24.0f, 18.0f, 10.0f, 220.0f, image.value);
    ege_pie(136.0f, 28.0f, 24.0f, 18.0f, 10.0f, 220.0f, image.value);
    ege_fillpie(50.0f, 52.0f, 24.0f, 18.0f, 10.0f, 220.0f, image.value);
    ege_roundrect(78.0f, 52.0f, 30.0f, 20.0f, 5.0f, image.value);
    ege_fillroundrect(112.0f, 52.0f, 30.0f, 20.0f, 5.0f, image.value);
    ege_roundrect(50.0f, 78.0f, 36.0f, 22.0f,
                  2.0f, 4.0f, 6.0f, 8.0f, image.value);
    ege_fillroundrect(92.0f, 78.0f, 36.0f, 22.0f,
                      2.0f, 4.0f, 6.0f, 8.0f, image.value);
    ege_enable_aa(true, image.value);

    EGE_CHECK(getpixel(90, 12, image.value) == kGreen);
    EGE_CHECK(getpixel(140, 13, image.value) == kGreen);
    EGE_CHECK(getpixel(90, 37, image.value) == kGreen);
    EGE_CHECK(getpixel(127, 62, image.value) == kGreen);
    EGE_CHECK(getpixel(110, 89, image.value) == kGreen);
    EGE_CHECK(countNotColor(image.value, kBackground) > 700);

    // Enhanced text has two public overloads. Empty strings make this a stable
    // headless link/argument contract without depending on an installed font.
    ege_drawtext("", 0.0f, 0.0f, image.value);
    ege_drawtext(L"", 0.0f, 0.0f, image.value);

    putpixel(0, 0, EGEARGB(255, 11, 22, 33), image.value);
    ege_setalpha(73, image.value);
    EGE_CHECK(getpixel(0, 0, image.value) == EGEARGB(73, 11, 22, 33));
    ege_setalpha(-20, image.value);
    EGE_CHECK(EGEGET_A(getpixel(0, 0, image.value)) == 0);
    ege_setalpha(999, image.value);
    EGE_CHECK(EGEGET_A(getpixel(0, 0, image.value)) == 255);

    const std::uint64_t beforeInvalid = checksum(image.value);
    ege_drawpoly(0, nullptr, image.value);
    ege_polyline(-1, nullptr, image.value);
    ege_polygon(0, nullptr, image.value);
    ege_fillpoly(0, nullptr, image.value);
    ege_bezier(0, nullptr, image.value);
    ege_drawcurve(0, nullptr, image.value);
    ege_fillrect(0.0f, 0.0f, -1.0f, 4.0f, image.value);
    ege_fillellipse(0.0f, 0.0f, 0.0f, 4.0f, image.value);
    EGE_CHECK(checksum(image.value) == beforeInvalid);
}

void testTransformsAndIsolation()
{
    Image first(80, 60);
    Image second(80, 60);
    EGE_CHECK(first.value != nullptr && second.value != nullptr);
    clearImage(first.value);
    clearImage(second.value);

    ege_transform_matrix identity{};
    ege_get_transform(&identity, first.value);
    EGE_CHECK(near(identity.m11, 1.0f) && near(identity.m22, 1.0f));
    EGE_CHECK(near(identity.m12, 0.0f) && near(identity.m21, 0.0f));
    EGE_CHECK(near(identity.m31, 0.0f) && near(identity.m32, 0.0f));

    ege_transform_translate(20.0f, 10.0f, first.value);
    ege_point translated = ege_transform_calc(2.0f, 3.0f, first.value);
    EGE_CHECK(near(translated.x, 22.0f) && near(translated.y, 13.0f));
    const ege_point untouched = ege_transform_calc(ege_point{2.0f, 3.0f}, second.value);
    EGE_CHECK(near(untouched.x, 2.0f) && near(untouched.y, 3.0f));

    setfillcolor(kRed, first.value);
    ege_fillrect(0.0f, 0.0f, 10.0f, 10.0f, first.value);
    EGE_CHECK(getpixel(25, 15, first.value) == kRed);
    EGE_CHECK(getpixel(5, 5, first.value) == kBackground);

    ege_transform_reset(first.value);
    ege_transform_rotate(90.0f, first.value);
    const ege_point rotated = ege_transform_calc(1.0f, 0.0f, first.value);
    EGE_CHECK(near(rotated.x, 0.0f) && near(rotated.y, 1.0f));
    ege_transform_translate(10.0f, 0.0f, first.value);
    const ege_point prepended = ege_transform_calc(0.0f, 0.0f, first.value);
    EGE_CHECK(near(prepended.x, 0.0f) && near(prepended.y, 10.0f));
    ege_transform_reset(first.value);
    ege_transform_scale(2.0f, 3.0f, first.value);
    const ege_point scaled = ege_transform_calc(2.0f, 3.0f, first.value);
    EGE_CHECK(near(scaled.x, 4.0f) && near(scaled.y, 9.0f));

    const ege_transform_matrix explicitMatrix = {
        1.5f, 0.25f, -0.5f, 2.0f, 7.0f, 9.0f
    };
    ege_set_transform(&explicitMatrix, first.value);
    ege_transform_matrix roundTrip{};
    ege_get_transform(&roundTrip, first.value);
    EGE_CHECK(near(roundTrip.m11, explicitMatrix.m11));
    EGE_CHECK(near(roundTrip.m12, explicitMatrix.m12));
    EGE_CHECK(near(roundTrip.m21, explicitMatrix.m21));
    EGE_CHECK(near(roundTrip.m22, explicitMatrix.m22));
    EGE_CHECK(near(roundTrip.m31, explicitMatrix.m31));
    EGE_CHECK(near(roundTrip.m32, explicitMatrix.m32));

    // A null matrix is ignored by the original GDI+ API. It must not reset a
    // previously configured transform in the portable fallback.
    ege_set_transform(nullptr, first.value);
    ege_get_transform(nullptr, first.value);
    const ege_point afterNull = ege_transform_calc(2.0f, 3.0f, first.value);
    EGE_CHECK(near(afterNull.x, 8.5f) && near(afterNull.y, 15.5f));
}

void testEnhancedSourceOverAndRasterization()
{
    Image image(48, 32);
    EGE_CHECK(image.value != nullptr);
    const color_t dark = EGERGB(20, 40, 60);
    setbkcolor(dark, image.value);
    cleardevice(image.value);
    setviewport(0, 0, 48, 32, 1, image.value);
    ege_transform_reset(image.value);
    ege_setpattern_none(image.value);
    ege_enable_aa(false, image.value);

    // GDI+ enhanced primitives are always SourceOver. A legacy XOR mode must
    // neither replace a half-alpha source nor toggle it back on the next draw.
    setwritemode(R2_XORPEN, image.value);
    setfillcolor(EGEARGB(128, 200, 0, 0), image.value);
    ege_fillrect(2.0f, 2.0f, 8.0f, 8.0f, image.value);
    const color_t firstFill = getpixel(5, 5, image.value);
    EGE_CHECK(EGEGET_A(firstFill) == 255);
    EGE_CHECK(EGEGET_R(firstFill) >= 108 && EGEGET_R(firstFill) <= 112);
    EGE_CHECK(EGEGET_G(firstFill) >= 19 && EGEGET_G(firstFill) <= 21);
    EGE_CHECK(EGEGET_B(firstFill) >= 29 && EGEGET_B(firstFill) <= 31);
    ege_fillrect(2.0f, 2.0f, 8.0f, 8.0f, image.value);
    const color_t secondFill = getpixel(5, 5, image.value);
    EGE_CHECK(EGEGET_R(secondFill) > EGEGET_R(firstFill));

    setlinecolor(EGEARGB(128, 0, 200, 0), image.value);
    setlinestyle(PS_SOLID, 0, 1, image.value);
    setlinewidth(1.0f, image.value);
    ege_line(2.5f, 15.5f, 30.5f, 15.5f, image.value);
    const color_t firstStroke = getpixel(12, 15, image.value);
    EGE_CHECK(EGEGET_G(firstStroke) >= 118 && EGEGET_G(firstStroke) <= 122);
    ege_line(2.5f, 15.5f, 30.5f, 15.5f, image.value);
    EGE_CHECK(EGEGET_G(getpixel(12, 15, image.value)) > EGEGET_G(firstStroke));

    // Float coordinates are sampled at pixel centers instead of being rounded
    // through the legacy integer RenderTarget path.
    setbkcolor(dark, image.value);
    cleardevice(image.value);
    setlinecolor(kYellow, image.value);
    ege_line(2.5f, 4.5f, 20.5f, 4.5f, image.value);
    EGE_CHECK(getpixel(10, 4, image.value) == kYellow);
    EGE_CHECK(getpixel(10, 3, image.value) == dark);
    EGE_CHECK(getpixel(10, 5, image.value) == dark);

    // AA uses subpixel coverage; the same diagonal-edge pixel changes from a
    // hard full fill to a partial SourceOver blend.
    setfillcolor(kRed, image.value);
    ege_enable_aa(false, image.value);
    ege_fillcircle(10.0f, 10.0f, 5.0f, image.value);
    const color_t hardEdge = getpixel(13, 13, image.value);
    setbkcolor(dark, image.value);
    cleardevice(image.value);
    ege_enable_aa(true, image.value);
    ege_fillcircle(10.0f, 10.0f, 5.0f, image.value);
    const color_t antialiasedEdge = getpixel(13, 13, image.value);
    EGE_CHECK(EGEGET_R(hardEdge) == EGEGET_R(kRed));
    EGE_CHECK(EGEGET_R(antialiasedEdge) > EGEGET_R(dark));
    EGE_CHECK(EGEGET_R(antialiasedEdge) < EGEGET_R(hardEdge));
    ege_enable_aa(false, image.value);
    setwritemode(R2_COPYPEN, image.value);
}

void testEnhancedStrokeCapsAndJoins()
{
    Image image(80, 80);
    EGE_CHECK(image.value != nullptr);
    clearImage(image.value);
    ege_enable_aa(false, image.value);
    setlinecolor(kYellow, image.value);
    setlinestyle(PS_SOLID, 0, 8, image.value);
    setlinewidth(8.0f, image.value);

    const auto clearStroke = [&] {
        setbkcolor(kBackground, image.value);
        cleardevice(image.value);
    };

    // Flat caps stop at the endpoint, square caps include the corner of the
    // half-width extension, and round caps exclude that corner but include the
    // axial extension.
    setlinecap(LINECAP_FLAT, image.value);
    ege_line(20.5f, 20.5f, 40.5f, 20.5f, image.value);
    EGE_CHECK(getpixel(20, 20, image.value) == kYellow);
    EGE_CHECK(getpixel(17, 20, image.value) == kBackground);

    clearStroke();
    setlinecap(LINECAP_SQUARE, image.value);
    ege_line(20.5f, 20.5f, 40.5f, 20.5f, image.value);
    EGE_CHECK(getpixel(17, 17, image.value) == kYellow);

    clearStroke();
    setlinecap(LINECAP_ROUND, image.value);
    ege_line(20.5f, 20.5f, 40.5f, 20.5f, image.value);
    EGE_CHECK(getpixel(17, 20, image.value) == kYellow);
    EGE_CHECK(getpixel(17, 17, image.value) == kBackground);

    const ege_point corner[3] = {
        {20.5f, 45.5f}, {40.5f, 45.5f}, {40.5f, 65.5f}
    };
    clearStroke();
    setlinecap(LINECAP_FLAT, image.value);
    setlinejoin(LINEJOIN_BEVEL, image.value);
    ege_polyline(3, corner, image.value);
    EGE_CHECK(getpixel(40, 45, image.value) == kYellow);
    EGE_CHECK(getpixel(43, 43, image.value) == kBackground);

    clearStroke();
    setlinejoin(LINEJOIN_ROUND, image.value);
    ege_polyline(3, corner, image.value);
    EGE_CHECK(getpixel(43, 43, image.value) == kYellow);

    clearStroke();
    setlinejoin(LINEJOIN_MITER, 1.0f, image.value);
    ege_polyline(3, corner, image.value);
    EGE_CHECK(getpixel(43, 42, image.value) == kBackground);

    clearStroke();
    setlinejoin(LINEJOIN_MITER, 4.0f, image.value);
    ege_polyline(3, corner, image.value);
    EGE_CHECK(getpixel(43, 42, image.value) == kYellow);
}

void testGradientGeometryAndTiling()
{
    Image image(140, 120);
    EGE_CHECK(image.value != nullptr);
    clearImage(image.value);

    ege_setpattern_lineargradient(10.0f, 0.0f, kRed,
                                  30.0f, 0.0f, kBlue, image.value);
    ege_fillrect(0.0f, 0.0f, 50.0f, 8.0f, image.value);
    EGE_CHECK(getpixel(14, 4, image.value) == getpixel(34, 4, image.value));

    const std::array<ege_point, 4> diamond = {{
        {60.0f, 10.0f}, {110.0f, 60.0f},
        {60.0f, 110.0f}, {10.0f, 60.0f}
    }};
    const std::array<color_t, 4> surround = {{kRed, kGreen, kBlue, kYellow}};
    ege_setpattern_pathgradient({60.0f, 60.0f}, EGERGB(0, 0, 0),
        static_cast<int>(diamond.size()), diamond.data(),
        static_cast<int>(surround.size()), surround.data(), image.value);
    ege_fillrect(0.0f, 8.0f, 140.0f, 112.0f, image.value);
    const color_t nearTop = getpixel(60, 12, image.value);
    const color_t nearRight = getpixel(107, 60, image.value);
    const color_t nearBottom = getpixel(60, 107, image.value);
    const color_t nearLeft = getpixel(12, 60, image.value);
    EGE_CHECK(EGEGET_R(nearTop) > EGEGET_G(nearTop) && EGEGET_R(nearTop) > EGEGET_B(nearTop));
    EGE_CHECK(EGEGET_G(nearRight) > EGEGET_R(nearRight) && EGEGET_G(nearRight) > EGEGET_B(nearRight));
    EGE_CHECK(EGEGET_B(nearBottom) > EGEGET_R(nearBottom) && EGEGET_B(nearBottom) > EGEGET_G(nearBottom));
    EGE_CHECK(EGEGET_R(nearLeft) > EGEGET_B(nearLeft) && EGEGET_G(nearLeft) > EGEGET_B(nearLeft));
    // The polygon constructor explicitly requests WrapModeTile.
    EGE_CHECK(getpixel(15, 60, image.value) == getpixel(115, 60, image.value));

    clearImage(image.value);
    ege_setpattern_ellipsegradient({20.0f, 60.0f}, kRed,
        10.0f, 40.0f, 60.0f, 40.0f, kBlue, image.value);
    ege_fillrect(10.0f, 40.0f, 60.0f, 40.0f, image.value);
    const color_t halfwayLeft = getpixel(14, 60, image.value);
    const color_t halfwayRight = getpixel(44, 60, image.value);
    EGE_CHECK(EGEGET_R(halfwayLeft) > 70 && EGEGET_B(halfwayLeft) > 70);
    EGE_CHECK(EGEGET_R(halfwayRight) > 70 && EGEGET_B(halfwayRight) > 70);
    EGE_CHECK(std::abs(static_cast<int>(EGEGET_R(halfwayLeft)) -
                       static_cast<int>(EGEGET_R(halfwayRight))) < 30);
    EGE_CHECK(std::abs(static_cast<int>(EGEGET_B(halfwayLeft)) -
                       static_cast<int>(EGEGET_B(halfwayRight))) < 30);
}

void testPatternsAndTextureLifetime()
{
    Image image(128, 96);
    EGE_CHECK(image.value != nullptr);
    clearImage(image.value);

    ege_setpattern_lineargradient(10.0f, 0.0f, kRed,
                                  110.0f, 0.0f, kBlue, image.value);
    ege_fillrect(10.0f, 4.0f, 100.0f, 20.0f, image.value);
    const color_t linearLeft = getpixel(14, 12, image.value);
    const color_t linearRight = getpixel(106, 12, image.value);
    EGE_CHECK(EGEGET_R(linearLeft) > EGEGET_B(linearLeft));
    EGE_CHECK(EGEGET_B(linearRight) > EGEGET_R(linearRight));
    EGE_CHECK(linearLeft != linearRight);

    const std::array<ege_point, 4> boundary = {{
        {10.0f, 30.0f}, {110.0f, 30.0f},
        {110.0f, 90.0f}, {10.0f, 90.0f}
    }};
    const std::array<color_t, 4> surround = {{kBlue, kBlue, kBlue, kBlue}};
    ege_setpattern_pathgradient({60.0f, 60.0f}, kRed,
                                static_cast<int>(boundary.size()), boundary.data(),
                                static_cast<int>(surround.size()), surround.data(), image.value);
    ege_fillrect(10.0f, 30.0f, 100.0f, 60.0f, image.value);
    const color_t pathCenter = getpixel(60, 60, image.value);
    const color_t pathEdge = getpixel(108, 60, image.value);
    EGE_CHECK(EGEGET_R(pathCenter) > EGEGET_B(pathCenter));
    EGE_CHECK(EGEGET_B(pathEdge) > EGEGET_R(pathEdge));

    ege_setpattern_ellipsegradient({32.0f, 64.0f}, kYellow,
                                   12.0f, 44.0f, 40.0f, 40.0f, kBlue, image.value);
    ege_fillcircle(32.0f, 64.0f, 20.0f, image.value);
    const color_t ellipseCenter = getpixel(32, 64, image.value);
    const color_t ellipseEdge = getpixel(50, 64, image.value);
    EGE_CHECK(EGEGET_R(ellipseCenter) > EGEGET_R(ellipseEdge));
    EGE_CHECK(EGEGET_B(ellipseEdge) > EGEGET_B(ellipseCenter));

    // The legacy solid-fill setters replace any active enhanced brush.
    ege_setpattern_lineargradient(112.0f, 4.0f, kRed,
                                  124.0f, 4.0f, kBlue, image.value);
    setfillcolor(kGreen, image.value);
    ege_fillrect(112.0f, 4.0f, 12.0f, 8.0f, image.value);
    EGE_CHECK(getpixel(118, 8, image.value) == kGreen);
    ege_setpattern_lineargradient(112.0f, 14.0f, kRed,
                                  124.0f, 14.0f, kBlue, image.value);
    setfillstyle(SOLID_FILL, kYellow, image.value);
    ege_fillrect(112.0f, 14.0f, 12.0f, 8.0f, image.value);
    EGE_CHECK(getpixel(118, 18, image.value) == kYellow);

    PIMAGE texture = newimage(2, 2);
    EGE_CHECK(texture != nullptr);
    putpixel(0, 0, kRed, texture);
    putpixel(1, 0, kGreen, texture);
    putpixel(0, 1, kBlue, texture);
    putpixel(1, 1, kYellow, texture);
    ege_gentexture(true, texture);
    ege_setpattern_texture(texture, 0.0f, 0.0f, 2.0f, 2.0f, image.value);

    // The destination brush owns a snapshot. Deleting its source must neither
    // dangle nor change the repeated texture used by a later draw.
    delimage(texture);
    ege_fillrect(0.0f, 0.0f, 8.0f, 8.0f, image.value);
    EGE_CHECK(getpixel(0, 0, image.value) == kRed);
    EGE_CHECK(getpixel(1, 0, image.value) == kGreen);
    EGE_CHECK(getpixel(0, 1, image.value) == kBlue);
    EGE_CHECK(getpixel(1, 1, image.value) == kYellow);
    EGE_CHECK(getpixel(2, 0, image.value) == kRed);

    ege_setpattern_none(image.value);
    setfillcolor(kGreen, image.value);
    ege_fillrect(116.0f, 84.0f, 8.0f, 8.0f, image.value);
    EGE_CHECK(getpixel(120, 88, image.value) == kGreen);
}

void testTextureAndImageTransfers()
{
    Image source(4, 4);
    Image target(48, 36);
    EGE_CHECK(source.value != nullptr && target.value != nullptr);
    clearImage(source.value);
    clearImage(target.value);
    setfillcolor(kRed, source.value);
    ege_fillrect(0.0f, 0.0f, 2.0f, 4.0f, source.value);
    setfillcolor(kBlue, source.value);
    ege_fillrect(2.0f, 0.0f, 2.0f, 4.0f, source.value);
    ege_gentexture(true, source.value);

    // Generated textures wrap a live premultiplied surface on Windows. Native
    // snapshots must refresh before use and must not multiply alpha twice.
    const color_t halfRedPremultiplied = color_premultiply(EGEARGB(128, 200, 0, 0));
    putpixel(0, 0, halfRedPremultiplied, source.value);

    ege_puttexture(source.value, 0.0f, 0.0f, 8.0f, 8.0f, target.value);
    EGE_CHECK(countNotColor(target.value, kBackground) >= 48);
    const unsigned int putTextureRed = EGEGET_R(getpixel(0, 0, target.value));
    EGE_CHECK(putTextureRed >= 95 && putTextureRed <= 106);
    EGE_CHECK(EGEGET_R(getpixel(1, 4, target.value)) > EGEGET_B(getpixel(1, 4, target.value)));
    EGE_CHECK(EGEGET_B(getpixel(6, 4, target.value)) > EGEGET_R(getpixel(6, 4, target.value)));
    putpixel(0, 0, kRed, source.value);

    clearImage(target.value);
    const ege_rect destination = {10.0f, 4.0f, 8.0f, 8.0f};
    ege_puttexture(source.value, destination, target.value);
    EGE_CHECK(getpixel(11, 8, target.value) != kBackground);
    EGE_CHECK(getpixel(2, 2, target.value) == kBackground);

    clearImage(target.value);
    const ege_rect blueHalf = {2.0f, 0.0f, 2.0f, 4.0f};
    ege_puttexture(source.value, destination, blueHalf, target.value);
    const color_t cropped = getpixel(14, 8, target.value);
    EGE_CHECK(EGEGET_B(cropped) > EGEGET_R(cropped));

    clearImage(target.value);
    // The setter synchronizes changes made after gentexture(), then owns the
    // captured source rectangle independently of the source IMAGE lifetime.
    putpixel(2, 0, halfRedPremultiplied, source.value);
    ege_setpattern_texture(source.value, 2.0f, 0.0f, 2.0f, 4.0f, target.value);
    ege_fillrect(0.0f, 0.0f, 6.0f, 4.0f, target.value);
    const unsigned int patternRed = EGEGET_R(getpixel(0, 0, target.value));
    EGE_CHECK(patternRed >= 95 && patternRed <= 106);
    EGE_CHECK(EGEGET_B(getpixel(1, 0, target.value)) > EGEGET_R(getpixel(1, 0, target.value)));
    const unsigned int repeatedPatternRed = EGEGET_R(getpixel(2, 0, target.value));
    EGE_CHECK(repeatedPatternRed >= 95 && repeatedPatternRed <= 106);
    putpixel(2, 0, kBlue, source.value);

    clearImage(target.value);
    ege_drawimage(source.value, 3, 5, target.value);
    EGE_CHECK(getpixel(3, 5, target.value) == kRed);
    EGE_CHECK(getpixel(6, 5, target.value) == kBlue);

    clearImage(target.value);
    ege_drawimage(source.value, 4, 6, 12, 8, 0, 0, 4, 4, target.value);
    EGE_CHECK(EGEGET_R(getpixel(5, 9, target.value)) > EGEGET_B(getpixel(5, 9, target.value)));
    EGE_CHECK(EGEGET_B(getpixel(14, 9, target.value)) > EGEGET_R(getpixel(14, 9, target.value)));

    ege_gentexture(false, source.value);
    clearImage(target.value);
    const std::uint64_t withoutTexture = checksum(target.value);
    ege_puttexture(source.value, 0.0f, 0.0f, 8.0f, 8.0f, target.value);
    EGE_CHECK(checksum(target.value) == withoutTexture);

    ege_puttexture(nullptr, 0.0f, 0.0f, 8.0f, 8.0f, target.value);
    ege_drawimage(nullptr, 0, 0, target.value);
    ege_drawimage(nullptr, 0, 0, 8, 8, 0, 0, 4, 4, target.value);
    EGE_CHECK(checksum(target.value) == withoutTexture);
}

void exercisePathMutationOverloads(const ege_point* curve, PIMAGE image)
{
    PathPtr path = makePath();
    PathPtr other = makePath();
    EGE_CHECK(path != nullptr && other != nullptr);
    if (!path || !other) {
        return;
    }

    ege_path_start(path.get());
    ege_path_addline(path.get(), 1.0f, 1.0f, 8.0f, 1.0f);
    ege_path_addarc(path.get(), 2.0f, 2.0f, 8.0f, 6.0f, 0.0f, 180.0f);
    ege_path_addpolyline(path.get(), 4, curve);
    ege_path_addbezier(path.get(), 4, curve);
    ege_path_addbezier(path.get(), 1.0f, 1.0f, 3.0f, 7.0f,
                       7.0f, 3.0f, 9.0f, 9.0f);
    ege_path_addcurve(path.get(), 4, curve);
    ege_path_addcurve(path.get(), 4, curve, 0.4f);
    ege_path_addcircle(path.get(), 14.0f, 14.0f, 3.0f);
    ege_path_addrect(path.get(), 20.0f, 4.0f, 7.0f, 6.0f);
    ege_path_addellipse(path.get(), 30.0f, 4.0f, 8.0f, 6.0f);
    ege_path_addpie(path.get(), 40.0f, 4.0f, 8.0f, 6.0f, 0.0f, 120.0f);
    ege_path_addtext(path.get(), 0.0f, 0.0f, "", 10.0f);
    ege_path_addtext(path.get(), 0.0f, 0.0f, L"", 10.0f);
    ege_path_addpolygon(path.get(), 4, curve);
    ege_path_addclosedcurve(path.get(), 4, curve);
    ege_path_addclosedcurve(path.get(), 4, curve, 0.4f);
    ege_path_close(path.get());
    ege_path_closeall(path.get());
    ege_path_setfillmode(path.get(), FILLMODE_WINDING);

    ege_path_addrect(other.get(), 50.0f, 5.0f, 6.0f, 5.0f);
    ege_path_addpath(path.get(), other.get(), false);
    EGE_CHECK(ege_path_pointcount(path.get()) > 20);

    ege_path_reverse(path.get());
    ege_path_flatten(path.get(), nullptr);
    const ege_transform_matrix identity = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    ege_path_flatten(path.get(), &identity, 0.25f);
    EGE_CHECK(ege_path_pointcount(path.get()) > 0);

    // Each destructive operation gets a clone so every overload is exercised
    // independently and a platform-specific approximation cannot cascade.
    PathPtr widened(ege_path_clone(path.get()), ege_path_destroy);
    PathPtr widenedFlat(ege_path_clone(path.get()), ege_path_destroy);
    PathPtr outlined(ege_path_clone(path.get()), ege_path_destroy);
    PathPtr outlinedFlat(ege_path_clone(path.get()), ege_path_destroy);
    PathPtr warped(ege_path_clone(path.get()), ege_path_destroy);
    PathPtr warpedFlat(ege_path_clone(path.get()), ege_path_destroy);
    EGE_CHECK(widened && widenedFlat && outlined && outlinedFlat && warped && warpedFlat);
    if (!(widened && widenedFlat && outlined && outlinedFlat && warped && warpedFlat)) {
        return;
    }
    ege_path_widen(widened.get(), 2.0f, nullptr);
    ege_path_widen(widenedFlat.get(), 2.0f, &identity, 0.25f);
    ege_path_outline(outlined.get(), nullptr);
    ege_path_outline(outlinedFlat.get(), &identity, 0.25f);
    const ege_rect warpRect = {0.0f, 0.0f, 60.0f, 30.0f};
    const ege_point warpPoints[4] = {
        {0.0f, 0.0f}, {60.0f, 2.0f}, {58.0f, 30.0f}, {2.0f, 28.0f}
    };
    ege_path_warp(warped.get(), warpPoints, 4, &warpRect, nullptr);
    ege_path_warp(warpedFlat.get(), warpPoints, 4, &warpRect, &identity, 0.25f);
    EGE_CHECK(ege_path_pointcount(widened.get()) > 0);
    EGE_CHECK(ege_path_pointcount(warped.get()) > 0);

    ege_drawpath(path.get(), image);
    ege_drawpath(path.get(), 2.0f, 3.0f, image);
    ege_fillpath(path.get(), image);
    ege_fillpath(path.get(), 2.0f, 3.0f, image);
}

bool hasVerticalEdgeAt(const ege_path* path, float x, float top, float bottom)
{
    const int count = ege_path_pointcount(path);
    if (count <= 1) return false;
    std::unique_ptr<ege_point[]> points(ege_path_getpathpoints(path));
    std::unique_ptr<unsigned char[]> types(ege_path_getpathtypes(path));
    if (!points || !types) return false;
    ege_point figureStart{};
    ege_point previous{};
    bool havePrevious = false;
    const auto matches = [&](const ege_point& first, const ege_point& second) {
        return near(first.x, x) && near(second.x, x) &&
            ((near(first.y, top) && near(second.y, bottom)) ||
             (near(first.y, bottom) && near(second.y, top)));
    };
    for (int index = 0; index < count; ++index) {
        if ((types[index] & 0x07) == 0) {
            figureStart = previous = points[index];
            havePrevious = true;
            continue;
        }
        if (havePrevious && matches(previous, points[index])) return true;
        previous = points[index];
        if ((types[index] & 0x80) != 0) {
            if (matches(previous, figureStart)) return true;
            havePrevious = false;
        }
    }
    return false;
}

void testPathGeometryParity()
{
    PathPtr bezier = makePath();
    EGE_CHECK(bezier != nullptr);
    const ege_point curve[4] = {
        {0.0f, 0.0f}, {8.0f, 0.0f}, {12.0f, 10.0f}, {20.0f, 10.0f}
    };
    ege_path_addbezier(bezier.get(), 4, curve);
    const ege_rect originalBounds = ege_path_getbounds(bezier.get());
    ege_path_reverse(bezier.get());
    EGE_CHECK(ege_path_pointcount(bezier.get()) == 4);
    std::unique_ptr<ege_point[]> reversedPoints(ege_path_getpathpoints(bezier.get()));
    std::unique_ptr<unsigned char[]> reversedTypes(ege_path_getpathtypes(bezier.get()));
    EGE_CHECK(reversedPoints != nullptr && reversedTypes != nullptr);
    if (reversedPoints && reversedTypes) {
        EGE_CHECK(near(reversedPoints[0].x, curve[3].x) && near(reversedPoints[0].y, curve[3].y));
        EGE_CHECK(near(reversedPoints[1].x, curve[2].x) && near(reversedPoints[1].y, curve[2].y));
        EGE_CHECK(near(reversedPoints[2].x, curve[1].x) && near(reversedPoints[2].y, curve[1].y));
        EGE_CHECK(near(reversedPoints[3].x, curve[0].x) && near(reversedPoints[3].y, curve[0].y));
        EGE_CHECK((reversedTypes[0] & 0x07) == 0);
        EGE_CHECK((reversedTypes[1] & 0x07) == 3 &&
                  (reversedTypes[2] & 0x07) == 3 &&
                  (reversedTypes[3] & 0x07) == 3);
    }
    const ege_rect reversedBounds = ege_path_getbounds(bezier.get());
    EGE_CHECK(near(reversedBounds.x, originalBounds.x));
    EGE_CHECK(near(reversedBounds.y, originalBounds.y));
    EGE_CHECK(near(reversedBounds.w, originalBounds.w));
    EGE_CHECK(near(reversedBounds.h, originalBounds.h));

    PathPtr warped = makePath();
    ege_path_addline(warped.get(), 50.0f, 50.0f, 100.0f, 50.0f);
    const ege_rect sourceRect = {0.0f, 0.0f, 100.0f, 100.0f};
    const ege_point trapezoid[4] = {
        {0.0f, 0.0f}, {100.0f, 0.0f}, {25.0f, 100.0f}, {75.0f, 100.0f}
    };
    ege_path_warp(warped.get(), trapezoid, 4, &sourceRect, nullptr);
    std::unique_ptr<ege_point[]> warpedPoints(ege_path_getpathpoints(warped.get()));
    EGE_CHECK(warpedPoints != nullptr);
    if (warpedPoints) {
        EGE_CHECK(near(warpedPoints[0].x, 50.0f, 0.05f));
        EGE_CHECK(near(warpedPoints[0].y, 200.0f / 3.0f, 0.05f));
        EGE_CHECK(near(warpedPoints[1].x, 250.0f / 3.0f, 0.05f));
        EGE_CHECK(near(warpedPoints[1].y, 200.0f / 3.0f, 0.05f));
    }

    PathPtr adjacent = makePath();
    ege_path_addrect(adjacent.get(), 0.0f, 0.0f, 10.0f, 10.0f);
    ege_path_addrect(adjacent.get(), 10.0f, 0.0f, 10.0f, 10.0f);
    ege_path_outline(adjacent.get(), nullptr);
    const ege_rect adjacentBounds = ege_path_getbounds(adjacent.get());
    EGE_CHECK(near(adjacentBounds.x, 0.0f) && near(adjacentBounds.y, 0.0f));
    EGE_CHECK(near(adjacentBounds.w, 20.0f) && near(adjacentBounds.h, 10.0f));
    EGE_CHECK(!hasVerticalEdgeAt(adjacent.get(), 10.0f, 0.0f, 10.0f));

    PathPtr windingDuplicates = makePath();
    ege_path_setfillmode(windingDuplicates.get(), FILLMODE_WINDING);
    ege_path_addrect(windingDuplicates.get(), 3.0f, 4.0f, 12.0f, 9.0f);
    ege_path_addrect(windingDuplicates.get(), 3.0f, 4.0f, 12.0f, 9.0f);
    ege_path_outline(windingDuplicates.get(), nullptr);
    const ege_rect windingBounds = ege_path_getbounds(windingDuplicates.get());
    EGE_CHECK(ege_path_pointcount(windingDuplicates.get()) >= 4);
    EGE_CHECK(near(windingBounds.x, 3.0f) && near(windingBounds.y, 4.0f));
    EGE_CHECK(near(windingBounds.w, 12.0f) && near(windingBounds.h, 9.0f));

    Image image(100, 80);
    clearImage(image.value);
    PathPtr corner = makePath();
    const ege_point cornerPoints[3] = {
        {20.0f, 20.0f}, {40.0f, 20.0f}, {40.0f, 40.0f}
    };
    ege_path_addpolyline(corner.get(), 3, cornerPoints);
    ege_path_widen(corner.get(), 8.0f, nullptr);
    setlinecolor(kYellow, image.value);
    setlinewidth(1.0f, image.value);
    ege_drawpath(corner.get(), image.value);
    EGE_CHECK(getpixel(40, 20, image.value) == kBackground);

    PathPtr transformedRect = makePath();
    ege_path_addrect(transformedRect.get(), 0.0f, 0.0f, 20.0f, 20.0f);
    ege_transform_translate(30.0f, 10.0f, image.value);
    setlinestyle(PS_SOLID, 0, 4, image.value);
    setlinewidth(4.0f, image.value);
    setlinecap(LINECAP_ROUND, image.value);
    setlinejoin(LINEJOIN_MITER, image.value);
    EGE_CHECK(ege_path_inpath(transformedRect.get(), 40.0f, 20.0f, image.value));
    EGE_CHECK(!ege_path_inpath(transformedRect.get(), 10.0f, 10.0f, image.value));
    EGE_CHECK(ege_path_instroke(transformedRect.get(), 30.0f, 20.0f, image.value));
    EGE_CHECK(!ege_path_instroke(transformedRect.get(), 40.0f, 20.0f, image.value));

    const ege_rect penBounds = ege_path_getbounds(transformedRect.get(), nullptr, image.value);
    EGE_CHECK(penBounds.x < 0.0f && penBounds.x > -3.0f);
    EGE_CHECK(penBounds.w > 20.0f && penBounds.w < 25.0f);

    PathPtr cappedLine = makePath();
    ege_path_addline(cappedLine.get(), 0.0f, 40.0f, 20.0f, 40.0f);
    setlinecap(LINECAP_SQUARE, image.value);
    EGE_CHECK(ege_path_instroke(cappedLine.get(), 29.0f, 50.0f, image.value));
    setlinecap(LINECAP_FLAT, image.value);
    setlinestyle(PS_DASH, 0, 1, image.value);
    setlinewidth(1.0f, image.value);
    EGE_CHECK(ege_path_instroke(cappedLine.get(), 31.0f, 50.0f, image.value));
    EGE_CHECK(!ege_path_instroke(cappedLine.get(), 33.5f, 50.0f, image.value));
    ege_transform_reset(image.value);

    // The Graphics overloads test device coordinates. The device transform is
    // the enhanced world transform followed by the viewport origin.
    PathPtr viewportRect = makePath();
    ege_path_addrect(viewportRect.get(), 0.0f, 0.0f, 20.0f, 20.0f);
    setviewport(20, 15, 100, 80, 1, image.value);
    ege_transform_translate(10.0f, 5.0f, image.value);
    setlinestyle(PS_SOLID, 0, 4, image.value);
    setlinewidth(4.0f, image.value);
    setlinecap(LINECAP_FLAT, image.value);
    EGE_CHECK(ege_path_inpath(viewportRect.get(), 40.0f, 30.0f, image.value));
    EGE_CHECK(!ege_path_inpath(viewportRect.get(), 20.0f, 10.0f, image.value));
    EGE_CHECK(ege_path_instroke(viewportRect.get(), 30.0f, 30.0f, image.value));
    EGE_CHECK(!ege_path_instroke(viewportRect.get(), 40.0f, 30.0f, image.value));
    setviewport(0, 0, 100, 80, 1, image.value);
    ege_transform_reset(image.value);

    PathPtr oneTurn = makePath();
    PathPtr twoTurns = makePath();
    ege_path_addarc(oneTurn.get(), 0.0f, 0.0f, 20.0f, 10.0f, 0.0f, 360.0f);
    ege_path_addarc(twoTurns.get(), 0.0f, 0.0f, 20.0f, 10.0f, 0.0f, 720.0f);
    EGE_CHECK(ege_path_pointcount(oneTurn.get()) == ege_path_pointcount(twoTurns.get()));
    const ege_rect oneTurnBounds = ege_path_getbounds(oneTurn.get());
    const ege_rect twoTurnBounds = ege_path_getbounds(twoTurns.get());
    EGE_CHECK(near(oneTurnBounds.w, twoTurnBounds.w));
    EGE_CHECK(near(oneTurnBounds.h, twoTurnBounds.h));
}

void testCoreTextPathOutlines()
{
    PathPtr narrow = makePath();
    PathPtr wide = makePath();
    PathPtr utf8 = makePath();
    PathPtr wideUnicode = makePath();
    PathPtr ring = makePath();
    EGE_CHECK(narrow && wide && utf8 && wideUnicode && ring);
    if (!(narrow && wide && utf8 && wideUnicode && ring)) return;

    ege_path_addtext(narrow.get(), 0.0f, 0.0f, L"ii", 32.0f);
    ege_path_addtext(wide.get(), 0.0f, 0.0f, L"WW", 32.0f, -1, nullptr,
        FONTSTYLE_BOLD | FONTSTYLE_ITALIC);
    const ege_rect narrowBounds = ege_path_getbounds(narrow.get());
    const ege_rect wideBounds = ege_path_getbounds(wide.get());
    EGE_CHECK(narrowBounds.w > 0.0f && narrowBounds.h > 0.0f);
#if defined(__APPLE__)
    EGE_CHECK(wideBounds.w > narrowBounds.w * 1.5f);
#endif

    ege_path_addtext(utf8.get(), 0.0f, 0.0f, "\xC3\xA9", 32.0f);
    ege_path_addtext(wideUnicode.get(), 0.0f, 0.0f, L"\u00E9", 32.0f);
    const ege_rect utf8Bounds = ege_path_getbounds(utf8.get());
    const ege_rect wideUnicodeBounds = ege_path_getbounds(wideUnicode.get());
    EGE_CHECK(utf8Bounds.w > 0.0f && wideUnicodeBounds.w > 0.0f);
    EGE_CHECK(near(utf8Bounds.w, wideUnicodeBounds.w, 0.5f));
    EGE_CHECK(near(utf8Bounds.h, wideUnicodeBounds.h, 0.5f));

    ege_path_addtext(ring.get(), 0.0f, 0.0f, L"O", 40.0f);
    const ege_rect ringBounds = ege_path_getbounds(ring.get());
    EGE_CHECK(ringBounds.w > 0.0f && ringBounds.h > 0.0f);
#if defined(__APPLE__)
    // A true glyph outline retains the counter in O; the rectangle placeholder
    // used by the old fallback reported this center as filled.
    EGE_CHECK(!ege_path_inpath(ring.get(), ringBounds.x + ringBounds.w * 0.5f,
        ringBounds.y + ringBounds.h * 0.42f));
#endif
}

void testPathContract()
{
    Image image(128, 96);
    EGE_CHECK(image.value != nullptr);
    clearImage(image.value);
    setlinecolor(kYellow, image.value);
    setfillcolor(kGreen, image.value);

    PathPtr rectangle = makePath();
    EGE_CHECK(rectangle != nullptr);
    if (!rectangle) {
        return;
    }
    EGE_CHECK(rectangle->data() != nullptr);
    EGE_CHECK(ege_path_pointcount(rectangle.get()) == 0);
    ege_path_addrect(rectangle.get(), 10.0f, 12.0f, 30.0f, 20.0f);
    EGE_CHECK(ege_path_pointcount(rectangle.get()) >= 4);
    const ege_point last = ege_path_lastpoint(rectangle.get());
    EGE_CHECK(last.x >= 10.0f && last.x <= 40.0f);
    EGE_CHECK(last.y >= 12.0f && last.y <= 32.0f);

    const ege_rect bounds = ege_path_getbounds(rectangle.get());
    EGE_CHECK(near(bounds.x, 10.0f));
    EGE_CHECK(near(bounds.y, 12.0f));
    EGE_CHECK(near(bounds.w, 30.0f));
    EGE_CHECK(near(bounds.h, 20.0f));
    EGE_CHECK(ege_path_inpath(rectangle.get(), 20.0f, 20.0f));
    EGE_CHECK(ege_path_inpath(rectangle.get(), 20.0f, 20.0f, image.value));
    EGE_CHECK(!ege_path_inpath(rectangle.get(), 2.0f, 2.0f));
    (void)ege_path_instroke(rectangle.get(), 10.0f, 12.0f);
    (void)ege_path_instroke(rectangle.get(), 10.0f, 12.0f, image.value);

    std::unique_ptr<ege_point[]> allocatedPoints(ege_path_getpathpoints(rectangle.get()));
    std::unique_ptr<unsigned char[]> allocatedTypes(ege_path_getpathtypes(rectangle.get()));
    EGE_CHECK(allocatedPoints != nullptr && allocatedTypes != nullptr);
    std::array<ege_point, 16> suppliedPoints{};
    std::array<unsigned char, 16> suppliedTypes{};
    EGE_CHECK(ege_path_getpathpoints(rectangle.get(), suppliedPoints.data()) == suppliedPoints.data());
    EGE_CHECK(ege_path_getpathtypes(rectangle.get(), suppliedTypes.data()) == suppliedTypes.data());

    ege_fillpath(rectangle.get(), image.value);
    EGE_CHECK(getpixel(20, 20, image.value) == kGreen);
    clearImage(image.value);
    setfillcolor(kGreen, image.value);
    ege_fillpath(rectangle.get(), 45.0f, 0.0f, image.value);
    EGE_CHECK(getpixel(65, 20, image.value) == kGreen);

    PathPtr clone(ege_path_clone(rectangle.get()), ege_path_destroy);
    EGE_CHECK(clone != nullptr && ege_path_pointcount(clone.get()) == ege_path_pointcount(rectangle.get()));
    ege_path copied(*rectangle);
    ege_path assigned;
    assigned = *rectangle;
    EGE_CHECK(ege_path_pointcount(&copied) == ege_path_pointcount(rectangle.get()));
    EGE_CHECK(ege_path_pointcount(&assigned) == ege_path_pointcount(rectangle.get()));

    const ege_transform_matrix shift = {1.0f, 0.0f, 0.0f, 1.0f, 5.0f, 7.0f};
    const ege_rect shiftedBounds = ege_path_getbounds(rectangle.get(), &shift);
    EGE_CHECK(near(shiftedBounds.x, 15.0f) && near(shiftedBounds.y, 19.0f));
    const ege_rect penBounds = ege_path_getbounds(rectangle.get(), &shift, image.value);
    EGE_CHECK(penBounds.w >= shiftedBounds.w && penBounds.h >= shiftedBounds.h);
    ege_path_transform(clone.get(), &shift);
    const ege_rect transformedBounds = ege_path_getbounds(clone.get());
    EGE_CHECK(near(transformedBounds.x, 15.0f) && near(transformedBounds.y, 19.0f));

    const ege_point triangle[] = {{2.0f, 2.0f}, {12.0f, 2.0f}, {7.0f, 10.0f}};
    const unsigned char triangleTypes[] = {0, 1, static_cast<unsigned char>(0x80 | 1)};
    PathPtr fromFactory(ege_path_createfrom(triangle, triangleTypes, 3), ege_path_destroy);
    ege_path fromConstructor(triangle, triangleTypes, 3);
    EGE_CHECK(fromFactory != nullptr && ege_path_pointcount(fromFactory.get()) == 3);
    EGE_CHECK(ege_path_pointcount(&fromConstructor) == 3);

    const ege_point curve[] = {
        {1.0f, 1.0f}, {5.0f, 9.0f}, {9.0f, 1.0f}, {13.0f, 9.0f}
    };
    exercisePathMutationOverloads(curve, image.value);

    ege_path_reset(&assigned);
    EGE_CHECK(ege_path_pointcount(&assigned) == 0);

    // Null path arguments are specified as harmless no-ops/default queries.
    ege_path_start(nullptr);
    ege_path_close(nullptr);
    ege_path_closeall(nullptr);
    ege_path_setfillmode(nullptr, FILLMODE_DEFAULT);
    ege_path_reset(nullptr);
    ege_path_reverse(nullptr);
    ege_path_transform(nullptr, &shift);
    ege_path_addpath(nullptr, rectangle.get(), false);
    ege_path_addline(nullptr, 0.0f, 0.0f, 1.0f, 1.0f);
    ege_drawpath(nullptr, image.value);
    ege_fillpath(nullptr, image.value);
    EGE_CHECK(ege_path_clone(nullptr) == nullptr);
    EGE_CHECK(ege_path_pointcount(nullptr) == 0);
    EGE_CHECK(!ege_path_inpath(nullptr, 0.0f, 0.0f));
    EGE_CHECK(!ege_path_instroke(nullptr, 0.0f, 0.0f));
    EGE_CHECK(ege_path_getpathpoints(nullptr) == nullptr);
    EGE_CHECK(ege_path_getpathtypes(nullptr) == nullptr);
    ege_path_destroy(nullptr);
}

} // namespace

void runEnhancedApiContract()
{
    testEnhancedPrimitivesAndAlpha();
    testTransformsAndIsolation();
    testEnhancedSourceOverAndRasterization();
    testEnhancedStrokeCapsAndJoins();
    testGradientGeometryAndTiling();
    testPatternsAndTextureLifetime();
    testTextureAndImageTransfers();
    testPathGeometryParity();
    testCoreTextPathOutlines();
    testPathContract();
}

} // namespace test
} // namespace ege

int main()
{
    ege::test::runEnhancedApiContract();
    return ege::test::finish("EGE enhanced API contract");
}
