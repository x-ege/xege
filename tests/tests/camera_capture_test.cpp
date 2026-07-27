#include "ege/camera_capture.h"
#include "../test_shutdown.h"

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expectFramePixelsEqual(const std::shared_ptr<ege::CameraFrame>& frame,
                            ege::PIMAGE image,
                            const std::string& message)
{
    if (!frame || !image || !frame->getData()) {
        expect(false, message + " (missing frame or image data)");
        return;
    }

    const int width = frame->getWidth();
    const int height = frame->getHeight();
    const int stride = frame->getLineSizeInBytes();
    if (width <= 0 || height <= 0 || stride <= 0) {
        expect(false, message + " (invalid frame dimensions or stride)");
        return;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * sizeof(ege::color_t);
    const auto* source = frame->getData();
    const auto* destination = reinterpret_cast<const unsigned char*>(ege::getbuffer(image));

    bool equal = static_cast<std::size_t>(stride) >= rowBytes &&
                 destination != nullptr;
    for (int y = 0; equal && y < height; ++y) {
        equal = std::memcmp(destination + static_cast<std::size_t>(y) * rowBytes,
                            source + static_cast<std::size_t>(y) * stride,
                            rowBytes) == 0;
    }
    expect(equal, message);
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--headless") != 0)) {
        std::cerr << "Usage: camera_capture_test [--headless]\n";
        return EXIT_FAILURE;
    }
    const bool headless = argc == 2;

    expect(ege::hasCameraCaptureModule(), "camera capture module is enabled");
    if (!headless) {
        ege::initgraph(64, 48,
                       ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE);
        if (!ege::getHWnd()) {
            std::cerr << "FAIL: unable to create hidden camera capture test window\n";
            shutdown_graphics_for_test();
            return EXIT_FAILURE;
        }
    }

    ege::CameraCapture camera;
    expect(!camera.open("/path/that/does/not/exist.mp4", false),
           "opening a missing video source fails");
    expect(!camera.isOpened(), "a failed open leaves the capture closed");

    // A negative index selects the platform default camera.  Keep this test
    // conditional so headless CI runners without capture devices can still
    // exercise the deterministic file-backed state-machine checks below.
    {
        ege::CameraCapture defaultCamera;
        const auto devices = defaultCamera.findDeviceNames();
        if (devices.count > 0) {
            const bool opened = defaultCamera.open(-1, false);
            if (opened) {
                expect(defaultCamera.isOpened(), "the default camera reports an open state");
                expect(!defaultCamera.isStarted(),
                       "open(-1, false) preserves autoStart=false");
                defaultCamera.close();
                expect(!defaultCamera.isOpened(), "the default camera closes cleanly");
            } else {
                std::cout << "SKIP: enumerated camera is unavailable or permission was denied\n";
            }
        } else {
            std::cout << "SKIP: no physical camera is available for open(-1, false)\n";
        }
    }

    camera.setFrameSize(320, 240);
    camera.setFrameRate(30.0);
    if (!camera.open(EGE_TEST_VIDEO_PATH, false)) {
        std::cerr << "FAIL: unable to open deterministic camera fixture: "
                  << EGE_TEST_VIDEO_PATH << '\n';
        if (!headless) shutdown_graphics_for_test();
        return EXIT_FAILURE;
    }

    expect(camera.isOpened(), "open(..., false) opens without starting capture");
    expect(!camera.isStarted(), "autoStart=false leaves capture stopped");

    const auto resolutions = camera.getDeviceSupportedResolutions();
    expect(resolutions.count > 0 && resolutions.info != nullptr,
           "an opened source reports at least one supported resolution");

    expect(camera.start(), "an explicitly opened source starts successfully");
    expect(camera.isStarted(), "isStarted reflects the active capture state");

    const std::shared_ptr<ege::CameraFrame> frame = camera.grabFrame(3000);
    expect(frame != nullptr, "the deterministic source produces a frame");
    if (frame) {
        expect(frame->getWidth() > 0 && frame->getHeight() > 0,
               "captured frame has positive dimensions");
        expect(frame->getData() != nullptr, "captured frame exposes pixel data");
        expect(frame->getLineSizeInBytes() >= frame->getWidth() * 4,
               "BGRA frame stride covers every pixel in a row");

        if (!headless) {
            ege::PIMAGE image = frame->getImage();
            expect(image != nullptr, "captured frame exposes an EGE image");
            expect(frame->getImage() == image, "getImage reuses the frame-owned image");
            if (image) {
                expect(ege::getwidth(image) == frame->getWidth() &&
                           ege::getheight(image) == frame->getHeight(),
                       "frame and EGE image dimensions match");
                expectFramePixelsEqual(frame, image,
                                       "getImage copies exactly the active BGRA pixels");
            }

            ege::PIMAGE copy = frame->copyImage();
            expect(copy != nullptr && copy != image, "copyImage returns an owned image copy");
            if (copy) {
                expect(ege::getwidth(copy) == frame->getWidth() &&
                           ege::getheight(copy) == frame->getHeight(),
                       "copied image preserves frame dimensions");
                expectFramePixelsEqual(frame, copy,
                                       "copyImage copies exactly the active BGRA pixels");
                ege::delimage(copy);
            }
        }
    }

    camera.stop();
    expect(!camera.isStarted(), "stop ends frame capture");
    expect(camera.isOpened(), "stop keeps the source open for restarting");

    expect(camera.start(), "a stopped capture can be restarted");
    expect(camera.isStarted(), "restart restores the active capture state");
    const std::shared_ptr<ege::CameraFrame> restartedFrame = camera.grabFrame(3000);
    expect(restartedFrame != nullptr, "a restarted capture produces another frame");

    camera.stop();
    expect(!camera.isStarted(), "the restarted capture can be stopped again");
    camera.close();
    expect(!camera.isOpened(), "close releases the capture source");

    expect(camera.open(EGE_TEST_VIDEO_PATH, false),
           "a closed CameraCapture object can reopen the same source");
    expect(camera.isOpened(), "reopen restores the open state");
    expect(!camera.isStarted(), "reopen still honors autoStart=false");
    expect(camera.start(), "the reopened source starts successfully");
    const std::shared_ptr<ege::CameraFrame> reopenedFrame = camera.grabFrame(3000);
    expect(reopenedFrame != nullptr, "the reopened source produces a frame");
    camera.close();
    expect(!camera.isOpened(), "the reopened source closes cleanly");
    if (!headless) {
        expect(shutdown_graphics_for_test(),
               "the camera test window and UI thread shut down cleanly");
    }

    if (failures != 0) {
        std::cerr << failures << " camera capture assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All camera capture assertions passed\n";
    return EXIT_SUCCESS;
}
