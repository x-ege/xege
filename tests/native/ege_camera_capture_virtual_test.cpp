#include "test_support.h"

#include <ege/camera_capture.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <string>

namespace
{
void checkGray(ege::color_t color, int minimum, int maximum)
{
    const int red = EGEGET_R(color);
    const int green = EGEGET_G(color);
    const int blue = EGEGET_B(color);
    EGE_CHECK(EGEGET_A(color) == 255);
    EGE_CHECK(red >= minimum && red <= maximum);
    EGE_CHECK(std::abs(red - green) <= 3);
    EGE_CHECK(std::abs(red - blue) <= 3);
}
}

int main()
{
    EGE_CHECK(ege::hasCameraCaptureModule());
    EGE_CHECK(std::filesystem::exists("/dev/video99"));

    ege::CameraCapture camera;
    auto devices = camera.findDeviceNames();
    EGE_CHECK(devices.count == 1);
    EGE_CHECK(devices.info != nullptr);
    if (devices.count > 0 && devices.info) {
        EGE_CHECK(std::string(devices.info[0].name) == "EGE Virtual Camera");
    }

    camera.setFrameSize(640, 480);
    camera.setFrameRate(30.0);
    EGE_CHECK(camera.open("/dev/video99", false));
    EGE_CHECK(camera.isOpened());

    auto resolutions = camera.getDeviceSupportedResolutions();
    EGE_CHECK(resolutions.count == 1);
    if (resolutions.count > 0 && resolutions.info) {
        EGE_CHECK(resolutions.info[0].width == 640);
        EGE_CHECK(resolutions.info[0].height == 480);
    }

    EGE_CHECK(camera.start());
    EGE_CHECK(camera.isStarted());
    std::shared_ptr<ege::CameraFrame> frame = camera.grabFrame(1000);
    EGE_CHECK(frame != nullptr);
    if (!frame) return ege_test::finish("EGE virtual camera capture");

    EGE_CHECK(frame->getWidth() == 640);
    EGE_CHECK(frame->getHeight() == 480);
    EGE_CHECK(frame->getLineSizeInBytes() >= 640 * 4);
    EGE_CHECK(frame->getData() != nullptr);

    ege::PIMAGE image = frame->getImage();
    EGE_CHECK(image != nullptr);
    if (image) {
        EGE_CHECK(ege::getwidth(image) == 640);
        EGE_CHECK(ege::getheight(image) == 480);
        checkGray(ege::getpixel(80, 80, image), 10, 35);
        checkGray(ege::getpixel(480, 80, image), 75, 105);
        checkGray(ege::getpixel(80, 360, image), 150, 180);
        checkGray(ege::getpixel(480, 360, image), 225, 255);

        const auto artifact = ege_test::artifacts() / "virtual-camera-frame.png";
        EGE_CHECK(ege::savepng(image, artifact.string().c_str(), false) == ege::grOk);
        EGE_CHECK(std::filesystem::is_regular_file(artifact));
        EGE_CHECK(std::filesystem::file_size(artifact) > 64);
    }

    ege::PIMAGE copy = frame->copyImage();
    EGE_CHECK(copy != nullptr);
    if (copy && image) {
        EGE_CHECK(ege_test::checksum(copy) == ege_test::checksum(image));
    }
    ege::delimage(copy);

    frame.reset();
    camera.stop();
    EGE_CHECK(!camera.isStarted());
    camera.close();
    EGE_CHECK(!camera.isOpened());
    return ege_test::finish("EGE virtual camera capture");
}
