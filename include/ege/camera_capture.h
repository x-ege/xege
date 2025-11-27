#pragma once
#ifndef EGE_CAMERA_CAPTURE_H
#define EGE_CAMERA_CAPTURE_H

// 判断一下是否支持 C++17, 不支持的给一个警告

#if !(defined(EGE_ENABLE_CAMERA_CAPTURE) && EGE_ENABLE_CAMERA_CAPTURE) && __cplusplus < 201703L

#pragma message( \
    "Warning: 你正在使用 EGE 的相机模块, 但是你的编译器设置为了较低的 C++ 标准. 如果提示找不到相关符号, 请考虑替换为支持 C++17 的编译器. 定义宏 EGE_ENABLE_CAMERA_CAPTURE=1 来忽略这个警告.")

// 防止中文乱码看不见, 英文版也打印一下
#pragma message( \
    "Warning: You are using the EGE camera module, but your compiler is set to a lower C++ standard. If you encounter missing symbols, please consider switching to a compiler that supports C++17. Define the macro EGE_ENABLE_CAMERA_CAPTURE=1 to ignore this warning.")

#endif

#include "ege.h"
#include <memory>

/// 强制相机输出格式为 BGRA, 适配 ege::Image.
/// FrameOrientation 限制为 TopToBottom, 适配 ege::Image.
/// Allocator 重新实现为 ege::Image 内部.
/// 本头文件尽量基于旧版本的 C++ 标准来编写, 以便使用者在旧版本的编译器上也能使用，
/// 这样只要编译器本身运行时支持，并且 link 到 ege 就可以了.

namespace ccap
{
class Provider;
struct VideoFrame;
} // namespace ccap

namespace ege
{
class CameraCapture;
struct FrameContainer;

/// @brief 检查是否启用了相机捕获模块. 这通常是编译时确定的。
/// 目前支持的版本为: 1. MSVC2022+, 2. 支持 C++17 的 MinGW 等.
/// @return 如果启用了相机捕获模块, 返回 true; 否则返回 false.
bool hasCameraCaptureModule();

/// 一个帧数据的容器, 做一些数据回收相关的工作, 防止反复分配内存.
/// 为了契合 ege::IMAGE 的设计, 这里的像素格式永远是 BGRA.
class CameraFrame
{
protected:
    /// @brief CameraFrame 的生命周期由 shared_ptr 完全管理, 请勿手动释放或 delete.
    virtual ~CameraFrame() = 0;

public:
    CameraFrame();

    /**
     * @brief 获取 CameraFrame 对应的 ege::IMAGE. 这里的 PIMAGE 是一个指针,
     *      但是它的内存是分配在 CameraFrame 内部的.
     *      这里的返回值 PIMAGE 的生命周期归 CameraFrame 管理.
     *
     * @return 一个指向 ege::IMAGE 的指针, 或者 nullptr (如果没有数据).
     * @note 这个方法连续调用多次会返回同一个指针, 没有额外的开销.
     *
     */
    virtual PIMAGE getImage() = 0;

    /**
     * @brief 作用跟 getImage 类似， 但是强制拷贝一份数据. 使用者需要自己取释放这个 IMAGE.
     *  如果希望在相机关闭之后继续使用这个数据, 请使用这个函数.
     *  可以用在类似于拍照之后继续进行更多处理的场景.
     *
     * @return 一个指向 ege::IMAGE 的指针, 或者 nullptr (如果没有数据).
     * @note 这个方法内部无缓存, 请注意释放内存. 也就是说调用者需要负责释放这个 IMAGE.
     */
    virtual PIMAGE copyImage() = 0;

    /// 获取原始数据
    virtual unsigned char* getData() = 0;

    /// 每一行的字节数, 不一定等于 width * 4, 可能是对齐的.
    virtual int getLineSizeInBytes() = 0;

    /// 获取原始数据图像的宽
    virtual int getWidth() const = 0;

    /// 获取原始数据图像的高
    virtual int getHeight() const = 0;

protected:
    CameraCapture*    m_camera;
    ccap::VideoFrame* m_frame;
};

class CameraCapture
{
public:
    CameraCapture();
    ~CameraCapture();

    struct DeviceInfo
    {
        char name[128]; ///< 设备名称
        // ... 后面也许会添加更多信息, 比如相机支持的分辨率, 像素格式等.
    };

    struct ResolutionInfo
    {
        int width;  ///< 分辨率宽度
        int height; ///< 分辨率高度
    };

    struct ResolutionList
    {
        ResolutionList() = default;

        ResolutionList(ResolutionInfo* _info, int _count) : info(_info), count(_count) {}

        ResolutionList(const ResolutionList&) = delete;

        ResolutionList(ResolutionList&& r) : info(r.info), count(r.count)
        {
            const_cast<ResolutionInfo*&>(r.info) = nullptr;
            const_cast<int&>(r.count)            = 0;
        }

        ResolutionList& operator=(const ResolutionList&) = delete;
        ~ResolutionList();
        const ResolutionInfo* info  = nullptr; ///< 分辨率信息(数组)
        const int             count = 0;       ///< 分辨率数量
    };

    struct DeviceList
    {
        DeviceList() = default;

        DeviceList(DeviceInfo* _info, int _count) : info(_info), count(_count) {}

        DeviceList(const DeviceList&) = delete;

        DeviceList(DeviceList&& d) : info(d.info), count(d.count)
        {
            const_cast<DeviceInfo*&>(d.info) = nullptr; // 避免析构时重复释放
            const_cast<int&>(d.count)        = 0;
        }

        DeviceList& operator=(const DeviceList&) = delete;
        ~DeviceList();
        const DeviceInfo* info  = nullptr; ///< 设备信息(数组)
        const int         count = 0;       ///< 设备数量
    };

    /**
     * @brief 查找所有可用的相机设备名称.
     * @note 这个函数会扫描所有可用的相机设备, 并返回一个 DeviceList 对象.
     *       这个 DeviceList 对象包含了所有设备的名称和数量.
     *       这里原本应该返回 `std::vector<std::string>`,
     *       但是 EGE 内部在 MSVC 下使用 `/MT` 编译来避免引入动态库依赖, 所以避免此类接口, 下同.
     * @return DeviceList 对象, 包含所有可用的相机设备名称. 注意, 目前只有名称.
     */
    DeviceList findDeviceNames();

    /**
     * @brief 获取当前相机设备支持的分辨率列表.
     * @note 必须在 `open` 成功之后调用, 否则返回空列表.
     * @return ResolutionList 对象, 包含所有支持的分辨率.
     */
    ResolutionList getDeviceSupportedResolutions();

    /**
     * @brief 设置期望的相机分辨率. 相机不一定支持这个物理分辨率, 只是会尽可能找一个跟这个分辨率接近的.
     * @note 不设置的话会随便给一个。 如果要获取真实的分辨率, 请在获得帧数据之后，直接从帧数据中获取.
     * @attention 请在 `open` 之前设置这个参数. `open` 之后设置这个参数无效.
     */
    void setFrameSize(int width, int height);

    /**
     * @brief 设置期望的相机帧率. 相机不一定支持这个物理帧率, 只是会尽可能找一个跟这个帧率接近的.
     * @note 不设置的话会随便给一个, 一般是 30.
     * @attention 请在 `open` 之前设置这个参数. `open` 之后设置这个参数无效.
     */
    void setFrameRate(double fps);

    /**
     * @brief 通过相机设备名称打开相机设备
     *
     * @param deviceName 相机设备名称, 可以通过 `findDeviceNames` 获取.
     *      传入空字符串表示自动选择.
     * @param autoStart open 成功后是否自动调用 start() 方法开始捕获图像.
     * @note 注意, 自动选择的设备不一定是 `findDeviceNames` 中的第一个设备.
     * @return 如果设备打开成功, 返回 true, 否则返回 false.
     */
    bool open(const char* deviceName, bool autoStart = true);

    /**
     * @brief 通过相机设备索引打开相机设备
     *
     * @param deviceIndex 相机设备索引, 可以通过 `findDeviceNames` 获取.
     *      传入 -1 表示自动选择, 传入自然数表示 `findDeviceNames` 找到的对应设备.
     *      如果传入的数超过了设备数量, 会选择最后一个设备.
     * @param autoStart open 成功后是否自动调用 start() 方法开始捕获图像.
     * @note 注意, 自动选择的设备不一定是 `findDeviceNames` 中的第一个设备.
     * 如果传入的数超过了设备数量, 会选择最后一个设备.
     * @return 如果设备打开成功, 返回 true, 否则返回 false.
     */
    bool open(int deviceIndex, bool autoStart = true);

    /**
     * @brief 关闭相机设备, 释放资源.
     * @note 调用这个方法后, CameraCapture 对象不应该再被使用.
     *       这个方法会在 CameraCapture 对象被销毁时自动调用.
     */
    void close();

    /**
     * @brief 检查相机设备是否已经打开.
     * @return 如果相机设备已经打开, 返回 true, 否则返回 false.
     */
    bool isOpened() const;

    /**
     * @brief 启动相机, 开始捕获图像.
     * @return 如果启动成功, 返回 true, 否则返回 false.
     */
    bool start();

    /**
     * @brief 检查相机是否正在捕获图像.
     * @return 如果相机正在捕获图像, 返回 true, 否则返回 false.
     */
    bool isStarted() const;

    /**
     * @brief 停止相机捕获图像.
     * @note 可以在调用 start() 后调用此方法来停止捕获图像.
     */
    void stop();

    /**
     * @brief 获取当前帧数据.
     * @param timeout 超时等待时间, 单位毫秒. 0 表示不等待, 直接返回.
     *
     * @return 当前帧数据的 shared_ptr. 如果等待超时或者没有数据, 返回空的 shared_ptr.
     *      无需手动释放, shared_ptr 会自动管理生命周期.
     *
     * @note BREAKING CHANGE: 返回类型已从 CameraFrame* 改为 std::shared_ptr<CameraFrame>.
     *       请更新代码以使用 shared_ptr 语义。这一改动提升了内存安全性和资源自动管理能力.
     */
    std::shared_ptr<CameraFrame> grabFrame(unsigned int timeoutInMs = 0);

    ////////////////////////////////////////////////

    // 内部实现, 使用者无需关心.
    FrameContainer* getFrameContainer() const;

private:
    ccap::Provider* m_provider;
    FrameContainer* m_frameContainer;
};

/// @brief  logLevel 0: 不输出日志, 1: 输出警告日志, 2: 输出常规信息, 3: 输出调试信息, 超过 3 等同于 3.
void enableCameraModuleLog(unsigned int logLevel);

} // namespace ege

// TODO: 看看后面要不要增加一套 C 语言的接口.

#endif
