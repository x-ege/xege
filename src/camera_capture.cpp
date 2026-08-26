#if __cplusplus >= 201103L

#include "ege/camera_capture.h"

#include <cstdio>
#include <cassert>

#if EGE_ENABLE_CAMERA_CAPTURE

#include <ccap.h>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <vector>

#include "image.h"

using namespace ccap;

#define CHECK_AND_PRINT_ERROR_MSG(...) (void(0))
#else
static const char* ERROR_MSG =
    "EGE 错误: 当前版本的EGE未包含相机模块! 请升级至更高的 ege(25.XX) 版本 (MSVC 2017+ 或者 MinGW, Cygwin, Clang++等的支持 C++17 及以上的版本)\n"
    "EGE Error: The current version of EGE does not include the camera module! Please upgrade to a higher version of ege (25.XX) (MSVC 2017+ or MinGW, Cygwin, Clang++ and other versions that support C++17 and above)\n";

// 为了适配 C++11 之前的编译器, 这里不能 `return {};`
#define CHECK_AND_PRINT_ERROR_MSG(...) \
    do {                               \
        fputs(ERROR_MSG, stderr);      \
        return __VA_ARGS__;            \
    } while (0)
#endif

namespace ege
{
class CameraFrameImp;

bool hasCameraCaptureModule()
{
#if EGE_ENABLE_CAMERA_CAPTURE
    return true;
#else
    return false;
#endif
}

// 仅仅是套壳, 避免把 std::shared_ptr 暴露出去.
struct FrameContainer
{
#if EGE_ENABLE_CAMERA_CAPTURE
    std::vector<std::shared_ptr<CameraFrameImp>> allFrames;
#else
    int dummy;
#endif
};

CameraFrame::CameraFrame() : m_camera(nullptr), m_frame(nullptr)
{}

CameraFrame::~CameraFrame()
{}

#if EGE_ENABLE_CAMERA_CAPTURE

class CameraFrameImp : public CameraFrame, public std::enable_shared_from_this<CameraFrameImp>
{
public:
    CameraFrameImp(CameraCapture* cam) { m_camera = cam; }

    ~CameraFrameImp()
    {
        // 析构时清空帧指针。由于 m_realFrame 是 shared_ptr, 其管理的资源会自动释放。
        // m_realImage 也是 shared_ptr，同样会自动清理。
        // 此处显式设置 m_frame 为 nullptr 是为了保持与基类成员的一致性。
        m_frame = nullptr;
    }

    PIMAGE getImage() override
    {
        // 检查是否需要更新图像数据：
        // 1. m_frame != m_realFrame.get() 表示帧数据已更新（新帧或帧被复用）
        // 2. m_realImage == nullptr 表示图像尚未创建
        if ((m_frame != m_realFrame.get() || m_realImage == nullptr) && m_realFrame) {
            if (m_realFrame->pixelFormat != ccap::PixelFormat::BGRA32) {
                fputs("ege: 抓取到的图像格式不正确, 请上报一个错误!!", stderr);
                fputs("ege: The captured image format is incorrect, please report an error!!", stderr);
                assert(0);
                return nullptr;
            }

            // 更新一下数据.
            m_frame = m_realFrame.get();
            if (!m_realImage) {
                m_realImage = std::make_shared<ege::IMAGE>(m_realFrame->width, m_realFrame->height, ege::BLACK);
            }

            auto* img    = m_realImage.get();
            auto* buffer = img->getbuffer();

            if (m_realFrame->width * 4 == m_realFrame->stride[0]) { // 已对齐
                assert(m_realFrame->sizeInBytes >= m_realFrame->width * m_realFrame->height * 4);
                memcpy(buffer, m_realFrame->data[0], m_realFrame->sizeInBytes);
            } else { // 未对齐
                for (uint32_t i = 0; i < m_realFrame->height; ++i) {
                    memcpy(buffer + i * m_realFrame->width, m_realFrame->data[0] + i * m_realFrame->stride[0],
                        m_realFrame->width * 4);
                }
            }
        }

        if (m_realImage) {
            return m_realImage.get();
        }

        return nullptr;
    }

    PIMAGE copyImage() override
    {
        if (m_realFrame) {
            if (m_realFrame->pixelFormat != ccap::PixelFormat::BGRA32) {
                fputs("ege: 抓取到的图像格式不正确, 请上报一个错误!!", stderr);
                fputs("ege: The captured image format is incorrect, please report an error!!", stderr);
                assert(0);
                return m_realImage.get();
            }

            PIMAGE img    = new ege::IMAGE(m_realFrame->width, m_realFrame->height, ege::BLACK);
            auto*  buffer = img->getbuffer();
            if (m_realFrame->width * 4 == m_realFrame->stride[0]) { // 已对齐
                memcpy(buffer, m_realFrame->data[0], m_realFrame->sizeInBytes);
            } else { // 未对齐
                for (uint32_t i = 0; i < m_realFrame->height; ++i) {
                    memcpy(buffer + i * m_realFrame->width, m_realFrame->data[0] + i * m_realFrame->stride[0],
                        m_realFrame->width * 4);
                }
            }
            return img;
        }
        return nullptr;
    }

    unsigned char* getData() override
    {
        if (m_realFrame) {
            return m_realFrame->data[0];
        }
        return nullptr;
    }

    int getLineSizeInBytes() override
    {
        if (m_realFrame) {
            return m_realFrame->stride[0];
        }
        return 0;
    }

    int getWidth() const override
    {
        if (m_realFrame) {
            return m_realFrame->width;
        }
        return 0;
    }

    int getHeight() const override
    {
        if (m_realFrame) {
            return m_realFrame->height;
        }
        return 0;
    }

    void setCcapFrame(std::shared_ptr<ccap::VideoFrame> frame) { m_realFrame = std::move(frame); }

    ccap::VideoFrame* getCcapFrame() const { return m_realFrame.get(); }

private:
    std::shared_ptr<ccap::VideoFrame> m_realFrame;
    std::shared_ptr<ege::IMAGE>       m_realImage;
};

#endif

/////////////////

CameraCapture::CameraCapture() : m_provider(nullptr), m_frameContainer(nullptr)
{
    CHECK_AND_PRINT_ERROR_MSG();

#if EGE_ENABLE_CAMERA_CAPTURE
    m_provider       = new Provider();
    m_frameContainer = new FrameContainer();
#endif
}

CameraCapture::~CameraCapture()
{
    CHECK_AND_PRINT_ERROR_MSG();

#if EGE_ENABLE_CAMERA_CAPTURE
    CameraCapture::close();
    if (m_provider != nullptr) {
        delete m_provider;
    }
    if (m_frameContainer != nullptr) {
        delete m_frameContainer;
    }
#endif
}

CameraCapture::DeviceList::~DeviceList()
{
    if (info != nullptr) {
        delete[] (DeviceInfo*)info;
        info = nullptr;
    }
}

CameraCapture::ResolutionList::~ResolutionList()
{
    if (info != nullptr) {
        delete[] (ResolutionInfo*)info;
        info = nullptr;
    }
}

CameraCapture::DeviceList CameraCapture::findDeviceNames()
{
    CHECK_AND_PRINT_ERROR_MSG({});
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider) {
        if (auto names = m_provider->findDeviceNames(); !names.empty()) {
            DeviceInfo* deviceInfos = new DeviceInfo[names.size()];
            for (size_t i = 0; i < names.size(); ++i) {
                strncpy(deviceInfos[i].name, names[i].c_str(), sizeof(deviceInfos[i].name) - 1);
                deviceInfos[i].name[sizeof(deviceInfos[i].name) - 1] = '\0'; // 确保字符串以 null 结尾
            }
            return DeviceList(deviceInfos, static_cast<int>(names.size()));
        }
    }
    return {};
#endif
}

CameraCapture::ResolutionList CameraCapture::getDeviceSupportedResolutions()
{
    CHECK_AND_PRINT_ERROR_MSG({});
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider && m_provider->isOpened()) {
        auto deviceInfo = m_provider->getDeviceInfo();
        if (deviceInfo && !deviceInfo->supportedResolutions.empty()) {
            auto& resolutions = deviceInfo->supportedResolutions;
            ResolutionInfo* resInfos = new ResolutionInfo[resolutions.size()];
            for (size_t i = 0; i < resolutions.size(); ++i) {
                resInfos[i].width  = static_cast<int>(resolutions[i].width);
                resInfos[i].height = static_cast<int>(resolutions[i].height);
            }
            return ResolutionList(resInfos, static_cast<int>(resolutions.size()));
        }
    }
    return {};
#endif
}

void CameraCapture::setFrameSize(int width, int height)
{
    CHECK_AND_PRINT_ERROR_MSG();
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider) {
        m_provider->set(PropertyName::Width, width);
        m_provider->set(PropertyName::Height, height);
    }
#endif
}

void CameraCapture::setFrameRate(double fps)
{
    CHECK_AND_PRINT_ERROR_MSG();
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider) {
        m_provider->set(PropertyName::FrameRate, fps);
    }
#endif
}

bool CameraCapture::open(const char* deviceName, bool autoStart)
{
    CHECK_AND_PRINT_ERROR_MSG(false);

#if EGE_ENABLE_CAMERA_CAPTURE

    if (m_provider && m_provider->isOpened()) {
        fputs("ege: 相机已经打开, 请勿重复打开!!", stderr);
        fputs("ege: The camera is already open, please do not open it again!!", stderr);
        return false;
    }

    if (!m_frameContainer) {
        m_frameContainer = new FrameContainer();
    }

    m_provider->set(PropertyName::PixelFormatOutput, ccap::PixelFormat::BGRA32);
    m_provider->set(PropertyName::PixelFormatInternal, ccap::PixelFormat::BGRA32);
    m_provider->set(PropertyName::FrameOrientation, ccap::FrameOrientation::TopToBottom);

    return m_provider->open(deviceName, autoStart);
#endif
}

bool CameraCapture::open(int index, bool autoStart)
{
    CHECK_AND_PRINT_ERROR_MSG(false);

#if EGE_ENABLE_CAMERA_CAPTURE

    if (index < 0) {
        return open("");
    }

    auto deviceNames = m_provider ? m_provider->findDeviceNames() : std::vector<std::string>();
    if (deviceNames.empty()) {
        return false;
    }

    if (index >= deviceNames.size()) {
        index = static_cast<int>(deviceNames.size()) - 1;
    }
    return open(deviceNames[index].c_str(), autoStart);
#endif
}

void CameraCapture::close()
{
    CHECK_AND_PRINT_ERROR_MSG();

#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_frameContainer) {
        m_frameContainer->allFrames.clear();
    }
    if (m_provider) {
        m_provider->close();
    }
#endif
}

bool CameraCapture::isOpened() const
{
    CHECK_AND_PRINT_ERROR_MSG(false);
#if EGE_ENABLE_CAMERA_CAPTURE
    return m_provider && m_provider->isOpened();
#endif
}

bool CameraCapture::start()
{
    CHECK_AND_PRINT_ERROR_MSG(false);
#if EGE_ENABLE_CAMERA_CAPTURE
    return m_provider && m_provider->start();
#endif
}

bool CameraCapture::isStarted() const
{
    CHECK_AND_PRINT_ERROR_MSG(false);
#if EGE_ENABLE_CAMERA_CAPTURE
    return m_provider && m_provider->isStarted();
#endif
}

void CameraCapture::stop()
{
    CHECK_AND_PRINT_ERROR_MSG();
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider) {
        m_provider->stop();
    }
#endif
}

std::shared_ptr<CameraFrame> CameraCapture::grabFrame(unsigned int timeoutInMs)
{
    CHECK_AND_PRINT_ERROR_MSG(std::shared_ptr<CameraFrame>());
#if EGE_ENABLE_CAMERA_CAPTURE
    if (m_provider && m_provider->isStarted()) {
        auto frame = m_provider->grab(timeoutInMs);
        if (frame && m_frameContainer) {
            auto& allFrames = m_frameContainer->allFrames;

            // 查找只被 allFrames 持有的空闲帧 (use_count() == 1 表示用户已释放其 shared_ptr)
            if (auto freeFrameIt = std::find_if(allFrames.begin(), allFrames.end(),
                    [](const auto& f) { return f.use_count() == 1; });
                freeFrameIt != allFrames.end())
            {
                (*freeFrameIt)->setCcapFrame(std::move(frame));
                return *freeFrameIt;
            } else {
                auto frameImp = std::make_shared<CameraFrameImp>(this);
                frameImp->setCcapFrame(std::move(frame));
                m_frameContainer->allFrames.push_back(frameImp);
                fputs("ege: new frame created!!\n", stderr);
                if (m_frameContainer->allFrames.size() > 100) {
                    fputs("ege: too many frames allocated, consider checking for memory leaks!!\n", stderr);
                }
                return frameImp;
            }
        }
    }
    return nullptr;
#endif
}

FrameContainer* CameraCapture::getFrameContainer() const
{
    return m_frameContainer;
}

void enableCameraModuleLog(unsigned int logLevel)
{
    CHECK_AND_PRINT_ERROR_MSG();

#if EGE_ENABLE_CAMERA_CAPTURE
    switch (logLevel) {
    case 0:
        ccap::setLogLevel(ccap::LogLevel::None);
        break;
    case 1:
        ccap::setLogLevel(ccap::LogLevel::Warning);
        break;
    case 2:
        ccap::setLogLevel(ccap::LogLevel::Info);
        break;
    case 3:
    default:
        ccap::setLogLevel(ccap::LogLevel::Verbose);
        break;
    }
#endif
}

} // namespace ege

#endif
