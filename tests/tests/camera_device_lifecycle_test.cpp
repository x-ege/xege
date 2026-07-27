#include "ege/camera_capture.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

} // namespace

int main() {
    expect(ege::hasCameraCaptureModule(), "camera capture module is enabled");
    ege::enableCameraModuleLog(0);

    ege::CameraCapture camera;
    expect(!camera.isOpened(), "a new capture is closed");
    expect(!camera.isStarted(), "a new capture is stopped");
    expect(!camera.start(), "starting before open fails");
    expect(!camera.grabFrame(1), "grabbing before start returns no frame");

    auto closedResolutions = camera.getDeviceSupportedResolutions();
    expect(closedResolutions.count == 0 && closedResolutions.info == nullptr, "a closed capture has no device resolutions");
    auto movedClosedResolutions = std::move(closedResolutions);
    expect(closedResolutions.count == 0 && closedResolutions.info == nullptr &&
               movedClosedResolutions.count == 0 &&
               movedClosedResolutions.info == nullptr,
           "an empty resolution list preserves ownership when moved");

    auto devices = camera.findDeviceNames();
    expect(devices.count >= 0, "device enumeration returns a valid count");
    expect((devices.count == 0) == (devices.info == nullptr), "device enumeration keeps count and storage consistent");
    const int deviceCount = devices.count;
    const ege::CameraCapture::DeviceInfo* deviceStorage = devices.info;
    auto movedDevices = std::move(devices);
    expect(devices.count == 0 && devices.info == nullptr,
           "moving a device list clears the source");
    expect(movedDevices.count == deviceCount &&
               movedDevices.info == deviceStorage,
           "moving a device list transfers its storage");

    camera.setFrameSize(320, 240);
    camera.setFrameRate(30.0);
    expect(!camera.open("/dev/xege-camera-that-does-not-exist", false), "opening an invalid device fails");
    expect(!camera.isOpened(), "a failed open leaves the capture closed");
    expect(!camera.isStarted(), "a failed open leaves the capture stopped");

    if (movedDevices.count > 0) {
        if (camera.open(0, false)) {
            expect(camera.isOpened(), "an available device reports an open state");
            expect(!camera.isStarted(), "open(..., false) does not start capture");
            const auto resolutions = camera.getDeviceSupportedResolutions();
            expect(resolutions.count >= 0 &&
                       ((resolutions.count == 0) == (resolutions.info == nullptr)),
                   "an opened device keeps its resolution list consistent");
            camera.close();
            expect(!camera.isOpened(), "an available device closes cleanly");
        } else {
            std::cout << "SKIP: enumerated camera is unavailable or permission was denied\n";
        }
    } else {
        std::cout << "SKIP: no physical camera is available\n";
    }

    camera.stop();
    camera.close();
    camera.close();
    expect(!camera.isOpened(), "close is idempotent");
    expect(!camera.isStarted(), "the capture remains stopped after repeated close");

    if (failures != 0) {
        std::cerr << failures << " camera lifecycle assertion(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All camera lifecycle assertions passed\n";
    return EXIT_SUCCESS;
}
