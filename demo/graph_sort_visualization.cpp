/**
 * @file graph_sort_visualization.cpp
 * @author wysaid (this@xege.org)
 * @brief 对一些常见的排序算法进行可视化演示
 * @version 0.1
 * @date 2025-07-12
 *
 */

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <graphics.h>
#include <algorithm>
#include <string>
#include <vector>
#include <random>
#include <functional>
#include <chrono>
#include <iterator>
#include <cstdint>
#include <stdexcept>

// 文本本地化宏定义
#ifdef _MSC_VER
// MSVC编译器使用中文文案
#define TEXT_WINDOW_TITLE        "排序算法可视化演示"
#define TEXT_CONTROLS_TITLE      "按键说明："
#define TEXT_CONTROLS_START      "S/空格/回车 - 开始排序当前算法"
#define TEXT_CONTROLS_SHUFFLE    "R/ESC - 重新打乱数组"
#define TEXT_CONTROLS_NEXT       "->  切换到下一个算法"
#define TEXT_CONTROLS_PREV       "<-  切换到上一个算法"
#define TEXT_CONTROLS_AUTO       "A - 自动演示所有算法"
#define TEXT_CONTROLS_SPEED      "+/= - 加速动画，-/_ - 减速动画"
#define TEXT_CONTROLS_EXIT       "ESC - 退出程序（排序中按ESC可中断）"
#define TEXT_CURRENT_ALGORITHM   "当前算法: %s"
#define TEXT_ARRAY_SORTED        "数组已排序"
#define TEXT_ARRAY_NOT_SORTED    "数组未排序"
#define TEXT_SPEED_INFO          "动画速度设置: %d ms 每次操作 (+ 加速, - 减速)"
#define TEXT_EXECUTING           "正在执行: %s"
#define TEXT_EXECUTING_BATCH     "正在执行: %s (%d/%d)"
#define TEXT_SORT_COMPLETE       "排序完成！"
#define TEXT_SORT_TIME           "排序用时: %ld 毫秒"
#define TEXT_SORT_TIME_SHORT     "用时: %ld ms"
#define TEXT_ARRAY_SHUFFLED      "数组已重新打乱"
#define TEXT_NEXT_ALGORITHM      "2秒后继续下一个算法..."
#define TEXT_SORTING_INTERRUPTED "排序已中断"
#define TEXT_READY_TO_SORT       "准备就绪，按 S 开始排序"
#define TEXT_SORTING_IN_PROGRESS "排序进行中..."
#define TEXT_AUTO_DEMO_COMPLETE  "自动演示完成！"
#define TEXT_BUBBLE_SORT         "冒泡排序"
#define TEXT_SELECTION_SORT      "选择排序"
#define TEXT_INSERTION_SORT      "插入排序"
#define TEXT_QUICK_SORT          "快速排序"
#define TEXT_MERGE_SORT          "归并排序"
#define TEXT_HEAP_SORT           "堆排序"
#define TEXT_SHELL_SORT          "希尔排序"
#define TEXT_RADIX_SORT          "基数排序"
#define TEXT_COUNTING_SORT       "计数排序"
#define TEXT_STD_SORT            "标准库排序"
#define TEXT_STD_STABLE_SORT     "标准库稳定排序"

#define TEXT_BUBBLE_COMPLEXITY        "冒泡排序 - 时间复杂度: O(n^2), 空间复杂度: O(1)"
#define TEXT_SELECTION_COMPLEXITY     "选择排序 - 时间复杂度: O(n^2), 空间复杂度: O(1)"
#define TEXT_INSERTION_COMPLEXITY     "插入排序 - 时间复杂度: O(n^2), 空间复杂度: O(1)"
#define TEXT_QUICK_COMPLEXITY         "快速排序 - 时间复杂度: O(n log n), 空间复杂度: O(log n)"
#define TEXT_MERGE_COMPLEXITY         "归并排序 - 时间复杂度: O(n log n), 空间复杂度: O(n)"
#define TEXT_HEAP_COMPLEXITY          "堆排序 - 时间复杂度: O(n log n), 空间复杂度: O(1)"
#define TEXT_SHELL_COMPLEXITY         "希尔排序 - 时间复杂度: O(n^1.5), 空间复杂度: O(1)"
#define TEXT_RADIX_COMPLEXITY         "基数排序 - 时间复杂度: O(kn), 空间复杂度: O(k+n)"
#define TEXT_COUNTING_COMPLEXITY      "计数排序 - 时间复杂度: O(n+k), 空间复杂度: O(k)"
#define TEXT_STD_COMPLEXITY           "标准库排序 - 通常为混合排序算法"
#define TEXT_STD_STABLE_COMPLEXITY    "标准库稳定排序 - 时间复杂度: O(n log n), 空间复杂度: O(n)"
#define TEXT_STD_PARTIAL_COMPLEXITY   "标准库部分排序 - 时间复杂度: O(n log k), 空间复杂度: O(1)"
#define TEXT_STD_MAKE_HEAP_COMPLEXITY "标准库堆排序 - 时间复杂度: O(n log n), 空间复杂度: O(1)"
#define TEXT_STATS_FORMAT             "当前算法: %s | 写入/赋值: %d 次 | 比较: %d 次 | 读取: %d 次 | 按 ESC 可中断排序"
#define TEXT_FONT_NAME                "宋体"
#else
// 非MSVC编译器使用英文文案
#define TEXT_WINDOW_TITLE         "Sort Algorithm Visualization"
#define TEXT_CONTROLS_TITLE       "Controls:"
#define TEXT_CONTROLS_START       "S/Space/Enter - Start sorting current algorithm"
#define TEXT_CONTROLS_SHUFFLE     "R/ESC - Shuffle array"
#define TEXT_CONTROLS_NEXT        "->  Switch to next algorithm"
#define TEXT_CONTROLS_PREV        "<-  Switch to previous algorithm"
#define TEXT_CONTROLS_AUTO        "A - Auto demo all algorithms"
#define TEXT_CONTROLS_SPEED       "+/= - Speed up animation, -/_ - Slow down animation"
#define TEXT_CONTROLS_EXIT        "ESC - Exit program (Press ESC during sorting to interrupt)"
#define TEXT_CURRENT_ALGORITHM    "Current Algorithm: %s"
#define TEXT_ARRAY_SORTED         "Array is sorted"
#define TEXT_ARRAY_NOT_SORTED     "Array is not sorted"
#define TEXT_SPEED_INFO           "Animation speed: %d ms per operation (+ faster, - slower)"
#define TEXT_EXECUTING            "Executing: %s"
#define TEXT_EXECUTING_BATCH      "Executing: %s (%d/%d)"
#define TEXT_SORT_COMPLETE        "Sort Complete!"
#define TEXT_SORT_TIME            "Sort Time: %ld ms"
#define TEXT_SORT_TIME_SHORT      "Time: %ld ms"
#define TEXT_ARRAY_SHUFFLED       "Array shuffled"
#define TEXT_NEXT_ALGORITHM       "Next algorithm in 2 seconds..."
#define TEXT_SORTING_INTERRUPTED  "Sorting interrupted"
#define TEXT_READY_TO_SORT        "Ready to sort, press S to start"
#define TEXT_SORTING_IN_PROGRESS  "Sorting in progress..."
#define TEXT_AUTO_DEMO_COMPLETE   "Auto demo complete!"
#define TEXT_BUBBLE_SORT          "Bubble Sort"
#define TEXT_SELECTION_SORT       "Selection Sort"
#define TEXT_INSERTION_SORT       "Insertion Sort"
#define TEXT_QUICK_SORT           "Quick Sort"
#define TEXT_MERGE_SORT           "Merge Sort"
#define TEXT_HEAP_SORT            "Heap Sort"
#define TEXT_SHELL_SORT           "Shell Sort"
#define TEXT_RADIX_SORT           "Radix Sort"
#define TEXT_COUNTING_SORT        "Counting Sort"
#define TEXT_STD_SORT             "Standard Sort"
#define TEXT_STD_STABLE_SORT      "Standard Stable Sort"
#define TEXT_BUBBLE_COMPLEXITY    "Bubble Sort - Time: O(n^2), Space: O(1)"
#define TEXT_SELECTION_COMPLEXITY "Selection Sort - Time: O(n^2), Space: O(1)"
#define TEXT_INSERTION_COMPLEXITY "Insertion Sort - Time: O(n^2), Space: O(1)"
#define TEXT_QUICK_COMPLEXITY     "Quick Sort - Time: O(n log n), Space: O(log n)"
#define TEXT_MERGE_COMPLEXITY     "Merge Sort - Time: O(n log n), Space: O(n)"
#define TEXT_HEAP_COMPLEXITY      "Heap Sort - Time: O(n log n), Space: O(1)"
#define TEXT_SHELL_COMPLEXITY     "Shell Sort - Time: O(n^1.5), Space: O(1)"
#define TEXT_RADIX_COMPLEXITY     "Radix Sort - Time: O(kn), Space: O(k+n)"
#define TEXT_COUNTING_COMPLEXITY  "Counting Sort - Time: O(n+k), Space: O(k)"

#define TEXT_STD_COMPLEXITY        "Standard Sort - Usually hybrid algorithm"
#define TEXT_STD_STABLE_COMPLEXITY "Standard Stable Sort - Time: O(n log n), Space: O(n)"

#define TEXT_STATS_FORMAT \
    "Algorithm: %s | Writes/Assigns: %d | Comparisons: %d | Reads: %d | Press ESC to interrupt sorting"
#define TEXT_FONT_NAME "Arial"
#endif

// 可视化参数
const int WINDOW_WIDTH  = 1200;
const int WINDOW_HEIGHT = 800;
const int ARRAY_SIZE    = 100;
const int BAR_WIDTH     = WINDOW_WIDTH / ARRAY_SIZE;

static int g_operationDelay = 50; // 用于调整每次操作（赋值、访问等）的动画延迟时间

// 前向声明
class VisualizationState;

/**
 * @class MyElement
 * @brief 用于排序可视化的元素类
 *
 * 这个类封装了排序元素，能够自动统计赋值、拷贝、交换等操作
 * 并在操作时触发可视化更新
 *
 * 统计计数说明：
 * - 写入/赋值次数：包括构造函数初始化、赋值操作符调用
 * - 读取次数：包括类型转换操作、getValue()调用等读取数据的操作
 * - 比较次数：包括所有比较操作符调用
 * - 交换操作：通过std::swap实现，包含1次读取+2次写入
 */
class MyElement
{
public:
    // 默认构造函数
    MyElement() : m_value(0), m_visualizerPtr(nullptr) {}

    // 带值构造函数
    MyElement(int value) : m_value(value), m_visualizerPtr(nullptr)
    {
        recordWrite(); // 构造函数初始化值算作写入操作
        notifyVisualization();
        if (g_operationDelay > 0 && m_enableVisualization) {
            api_sleep(g_operationDelay);
        }
    }

    // 拷贝构造函数
    MyElement(const MyElement& other) : m_value(other.getValue()), m_visualizerPtr(other.m_visualizerPtr)
    {
        recordWrite(); // 拷贝构造函数算作写入操作
        notifyVisualization();
        if (g_operationDelay > 0 && m_enableVisualization) {
            api_sleep(g_operationDelay);
        }
    }

    // 赋值操作符
    MyElement& operator=(const MyElement& other)
    {
        if (this != &other) {
            m_value         = other.getValue();
            m_visualizerPtr = other.m_visualizerPtr;
            recordWrite(); // 赋值算作一种数据写入
            notifyVisualization();
            if (g_operationDelay > 0 && m_enableVisualization) {
                api_sleep(g_operationDelay);
            }
        }
        return *this;
    }

    // 从 int 赋值
    MyElement& operator=(int value)
    {
        m_value = value;
        recordWrite();
        notifyVisualization();
        if (g_operationDelay > 0 && m_enableVisualization) {
            api_sleep(g_operationDelay);
        }
        return *this;
    }

    // 比较操作符
    bool operator<(const MyElement& other) const
    {
        recordComparison();
        return m_value < other.m_value;
    }

    bool operator>(const MyElement& other) const
    {
        recordComparison();
        return m_value > other.m_value;
    }

    bool operator<=(const MyElement& other) const
    {
        recordComparison();
        return m_value <= other.m_value;
    }

    bool operator>=(const MyElement& other) const
    {
        recordComparison();
        return m_value >= other.m_value;
    }

    bool operator==(const MyElement& other) const
    {
        recordComparison();
        return m_value == other.m_value;
    }

    bool operator!=(const MyElement& other) const
    {
        recordComparison();
        return m_value != other.m_value;
    }

    // 与 int 的比较操作
    bool operator<(int value) const
    {
        recordComparison();
        return m_value < value;
    }

    bool operator>(int value) const
    {
        recordComparison();
        return m_value > value;
    }

    bool operator<=(int value) const
    {
        recordComparison();
        return m_value <= value;
    }

    bool operator>=(int value) const
    {
        recordComparison();
        return m_value >= value;
    }

    bool operator==(int value) const
    {
        recordComparison();
        return m_value == value;
    }

    bool operator!=(int value) const
    {
        recordComparison();
        return m_value != value;
    }

    // 类型转换操作符
    operator int() const
    {
        recordAccess();
        return m_value;
    }

    // 获取值
    int getValue() const
    {
        recordAccess();
        return m_value;
    }

    // 设置可视化器
    void setVisualizer(VisualizationState* visualizer) { m_visualizerPtr = visualizer; }

    // 获取可视化器
    VisualizationState* getVisualizer() const { return m_visualizerPtr; }

    // 统计相关的静态方法
    static void resetStats()
    {
        s_writeCount   = 0;
        s_compareCount = 0;
        s_readCount    = 0;
    }

    static int getWriteCount() { return s_writeCount; }

    static int getCompareCount() { return s_compareCount; }

    static int getReadCount() { return s_readCount; }

    static bool visualizationEnabled() { return m_enableVisualization; }

    static void enableVisualization(bool enable) { m_enableVisualization = enable; }

private:
    int                 m_value;
    VisualizationState* m_visualizerPtr;

    // 静态统计变量
    static int  s_writeCount;
    static int  s_compareCount;
    static int  s_readCount;           // 读取次数
    static bool m_enableVisualization; // 是否启用统计

    // 统计方法
    void recordWrite() const
    {
        if (m_enableVisualization) {
            ++s_writeCount;
        }
    }

    void recordComparison() const
    {
        if (m_enableVisualization) {
            ++s_compareCount;
            ++s_readCount; // 比较操作会读取元素值
        }
    }

    void recordAccess() const
    {
        if (m_enableVisualization) {
            ++s_readCount; // 记录读取次数
        }
    }

    // 通知可视化更新
    void notifyVisualization() const;
};

// 静态成员变量定义
int  MyElement::s_writeCount          = 0;
int  MyElement::s_compareCount        = 0;
int  MyElement::s_readCount           = 0;
bool MyElement::m_enableVisualization = false;

// 为 MyElement 添加 swap 特化
namespace std
{
template <> void swap(MyElement& a, MyElement& b)
{
    MyElement temp = a; // 拷贝构造函数：创建临时对象（1次写入）

    a = b;    // 赋值操作符：将b的值写入a（1次写入）
    b = temp; // 赋值操作符：将temp的值写入b（1次写入）

    // 总计：3次写入 - 这正确反映了swap的实际操作成本
    // 注意：读取操作应该通过元素的访问操作来统计
}
} // namespace std

/**
 * @class MyArray
 * @brief 用于排序可视化的数组类
 *
 * 这个类封装了用于排序的数组，并提供可视化功能
 * 支持任意排序算法的可视化，通过自定义迭代器实现
 */
class MyArray
{
public:
    /**
     * @class MyIterator
     * @brief 自定义迭代器，用于在排序过程中进行可视化
     *
     * 这个迭代器包装了vector的迭代器，在进行交换和比较操作时
     * 会触发可视化更新
     */
    class MyIterator
    {
    public:
        // 迭代器类型定义
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = MyElement;
        using difference_type   = std::ptrdiff_t;
        using pointer           = MyElement*;
        using reference         = MyElement&;

        MyIterator(MyArray* arr, int idx) : m_arrayPtr(arr), m_index(idx) {}

        // 解引用操作
        reference operator*()
        {
            m_arrayPtr->m_highlightedIndex1 = m_index;
            m_arrayPtr->notifyVisualization();
            return m_arrayPtr->m_data[m_index];
        }

        // const 版本的解引用操作
        const reference operator*() const
        {
            m_arrayPtr->m_highlightedIndex1 = m_index;
            m_arrayPtr->notifyVisualization();
            return m_arrayPtr->m_data[m_index];
        }

        pointer operator->() { return &(m_arrayPtr->m_data[m_index]); }

        const pointer operator->() const { return &(m_arrayPtr->m_data[m_index]); }

        // 前置递增
        MyIterator& operator++()
        {
            ++m_index;
            return *this;
        }

        // 后置递增
        MyIterator operator++(int)
        {
            MyIterator temp = *this;
            ++m_index;
            return temp;
        }

        // 前置递减
        MyIterator& operator--()
        {
            --m_index;
            return *this;
        }

        // 后置递减
        MyIterator operator--(int)
        {
            MyIterator temp = *this;
            --m_index;
            return temp;
        }

        // 随机访问操作
        MyIterator operator+(difference_type n) const { return MyIterator(m_arrayPtr, m_index + n); }

        MyIterator operator-(difference_type n) const { return MyIterator(m_arrayPtr, m_index - n); }

        MyIterator& operator+=(difference_type n)
        {
            m_index += n;
            return *this;
        }

        MyIterator& operator-=(difference_type n)
        {
            m_index -= n;
            return *this;
        }

        difference_type operator-(const MyIterator& other) const { return m_index - other.m_index; }

        // 下标操作
        reference operator[](difference_type n)
        {
            m_arrayPtr->m_highlightedIndex1 = m_index + n;
            m_arrayPtr->notifyVisualization();
            return m_arrayPtr->m_data[m_index + n];
        }

        // const 版本的下标操作
        const reference operator[](difference_type n) const
        {
            m_arrayPtr->m_highlightedIndex1 = m_index + n;
            m_arrayPtr->notifyVisualization();
            return m_arrayPtr->m_data[m_index + n];
        }

        // 比较操作
        bool operator==(const MyIterator& other) const { return m_index == other.m_index; }

        bool operator!=(const MyIterator& other) const { return m_index != other.m_index; }

        bool operator<(const MyIterator& other) const { return m_index < other.m_index; }

        bool operator>(const MyIterator& other) const { return m_index > other.m_index; }

        bool operator<=(const MyIterator& other) const { return m_index <= other.m_index; }

        bool operator>=(const MyIterator& other) const { return m_index >= other.m_index; }

        // 获取索引（用于调试）
        int getIndex() const { return m_index; }

        // 获取数组指针（用于交换函数）
        MyArray* getArray() const { return m_arrayPtr; }

    private:
        MyArray* m_arrayPtr;
        int      m_index;
    };

    // 构造函数
    MyArray(int size = ARRAY_SIZE) : m_data(size), m_visualizerPtr(nullptr)
    {
        // 初始化数组为随机值
        std::random_device              rd;
        std::mt19937                    gen(rd());
        std::uniform_int_distribution<> dis(10, WINDOW_HEIGHT - 50);

        MyElement::enableVisualization(false);

        for (size_t i = 0; i < static_cast<size_t>(size); ++i) {
            m_data[i] = MyElement(dis(gen));
            m_data[i].setVisualizer(nullptr); // 稍后会设置
        }

        MyElement::enableVisualization(true);
    }

    // 设置可视化状态引用
    void setVisualizer(VisualizationState* visualizer)
    {
        m_visualizerPtr = visualizer;
        // 为所有元素设置可视化器
        for (auto& element : m_data) {
            element.setVisualizer(visualizer);
        }
    }

    // 获取迭代器
    MyIterator begin() { return MyIterator(this, 0); }

    MyIterator end() { return MyIterator(this, m_data.size()); }

    // 获取大小
    size_t size() const { return m_data.size(); }

    // 获取数组数据（用于可视化）
    const std::vector<MyElement>& getData() const { return m_data; }

    // 获取高亮索引（用于可视化）
    int getHighlightedIndex1() const { return m_highlightedIndex1; }

    int getHighlightedIndex2() const { return m_highlightedIndex2; }

    // 获取当前算法名称（用于可视化）
    const std::string& getCurrentAlgorithm() const { return m_currentAlgorithm; }

    // 清除高亮（由可视化类调用）
    void clearHighlight()
    {
        m_highlightedIndex1 = -1;
        m_highlightedIndex2 = -1;
    }

    // 打乱数组
    void shuffle()
    {
        std::random_device rd;
        std::mt19937       gen(rd());
        std::shuffle(m_data.begin(), m_data.end(), gen);
        resetStats(); // 重置统计信息
        notifyVisualization();
    }

    // 检查是否已排序
    bool isSorted() const
    {
        for (size_t i = 1; i < m_data.size(); ++i) {
            if (m_data[i].getValue() < m_data[i - 1].getValue()) {
                return false;
            }
        }
        return true;
    }

    // 重置统计信息
    void resetStats()
    {
        clearHighlight();
        MyElement::resetStats();
    }

    // 设置当前算法名称
    void setCurrentAlgorithm(const std::string& algorithmName)
    {
        m_currentAlgorithm = algorithmName;
        resetStats();
    }

    // 获取统计信息
    int getWriteCount() const { return MyElement::getWriteCount(); }

    int getCompareCount() const { return MyElement::getCompareCount(); }

    int getReadCount() const { return MyElement::getReadCount(); }

    // 高亮两个元素（用于比较可视化）
    void highlightElements(int idx1, int idx2 = -1)
    {
        m_highlightedIndex1 = idx1;
        m_highlightedIndex2 = idx2;
    }

    // 通知可视化更新
    void notifyVisualization();

private:
    std::vector<MyElement> m_data;

    int m_highlightedIndex1 = -1; // 高亮的第一个元素
    int m_highlightedIndex2 = -1; // 高亮的第二个元素

    VisualizationState* m_visualizerPtr = nullptr; // 可视化状态引用

    std::string m_currentAlgorithm = ""; // 当前算法名称
};

// 为MyArray的迭代器添加交换函数特化
namespace std
{
template <> void iter_swap(MyArray::MyIterator a, MyArray::MyIterator b)
{
    // 高亮要交换的元素
    MyArray* arr = a.getArray();
    if (arr && a.getIndex() < arr->size() && b.getIndex() < arr->size()) {
        arr->highlightElements(a.getIndex(), b.getIndex());
    }

    // 执行交换 - MyElement 的赋值操作符会自动处理统计和可视化
    std::swap(*a, *b);
}
} // namespace std

// 排序算法实现

/**
 * @brief 冒泡排序
 */
template <typename Iterator> void bubbleSort(Iterator first, Iterator last)
{
    for (auto i = first; i != last; ++i) {
        for (auto j = first; j != last - 1; ++j) {
            if (*j > *(j + 1)) {
                std::iter_swap(j, j + 1);
            }
        }
    }
}

/**
 * @brief 选择排序
 */
template <typename Iterator> void selectionSort(Iterator first, Iterator last)
{
    for (auto i = first; i != last; ++i) {
        auto minIter = i;
        for (auto j = i + 1; j != last; ++j) {
            if (*j < *minIter) {
                minIter = j;
            }
        }
        if (minIter != i) {
            std::iter_swap(i, minIter);
        }
    }
}

/**
 * @brief 插入排序
 */
template <typename Iterator> void insertionSort(Iterator first, Iterator last)
{
    auto* arr = first.getArray();

    for (auto i = first + 1; i != last; ++i) {
        auto key = *i;
        auto j   = i - 1;

        while (j >= first && *j > key) {
            arr->highlightElements(j - first, j - first + 1); // 高亮交换的元素
            *(j + 1) = *j;
            --j;
        }
        *(j + 1) = key;
    }
    arr->clearHighlight(); // 清除高亮
}

/**
 * @brief 快速排序辅助函数 - 分区
 */
template <typename Iterator> Iterator partition(Iterator first, Iterator last)
{
    auto pivot = *(last - 1);
    auto i     = first - 1;

    for (auto j = first; j != last - 1; ++j) {
        if (*j <= pivot) {
            ++i;
            if (i != j) {
                std::iter_swap(i, j);
            }
        }
    }
    std::iter_swap(i + 1, last - 1);
    return i + 1;
}

/**
 * @brief 快速排序
 */
template <typename Iterator> void quickSort(Iterator first, Iterator last)
{
    if (first < last) {
        auto pivot = partition(first, last);
        quickSort(first, pivot);
        quickSort(pivot + 1, last);
    }
}

/**
 * @brief 归并排序辅助函数 - 合并
 */
template <typename Iterator> void merge(Iterator first, Iterator mid, Iterator last)
{
    std::vector<std::pair<Iterator, typename Iterator::value_type>> temp;

    auto left  = first;
    auto right = mid;

    while (left != mid && right != last) {
        if (*left <= *right) {
            temp.emplace_back(left, *left);
            ++left;
        } else {
            temp.emplace_back(right, *right);
            ++right;
        }
    }

    while (left != mid) {
        temp.emplace_back(left, *left);
        ++left;
    }

    while (right != last) {
        temp.emplace_back(right, *right);
        ++right;
    }

    auto* arr = first.getArray();

    // 复制回原数组
    auto iter = first;
    for (const auto& val : temp) {
        arr->highlightElements(iter.getIndex(), val.first.getIndex()); // 高亮当前元素
        *iter = val.second;
        ++iter;
    }
}

/**
 * @brief 归并排序
 */
template <typename Iterator> void mergeSort(Iterator first, Iterator last)
{
    auto distance = std::distance(first, last);
    if (distance > 1) {
        auto mid = first + distance / 2;
        mergeSort(first, mid);
        mergeSort(mid, last);
        merge(first, mid, last);
    }
}

/**
 * @brief 堆排序
 */
template <typename Iterator> void heapSort(Iterator first, Iterator last)
{
    // 手动实现堆排序以便与自定义迭代器兼容
    auto heapify = [](Iterator start, Iterator end, Iterator root) {
        while (true) {
            auto largest = root;
            auto left    = start + 2 * (root - start) + 1;
            auto right   = start + 2 * (root - start) + 2;

            if (left < end && (*left) > *largest) {
                largest = left;
            }

            if (right < end && (*right) > *largest) {
                largest = right;
            }

            if (largest == root) {
                break;
            }

            std::iter_swap(root, largest);
            root = largest;
        }
    };

    // 构建堆
    auto distance = std::distance(first, last);
    for (int i = distance / 2 - 1; i >= 0; i--) {
        heapify(first, last, first + i);
    }

    // 提取元素
    for (auto it = last - 1; it != first; --it) {
        std::iter_swap(first, it);
        heapify(first, it, first);
    }
}

/**
 * @brief 希尔排序
 */
template <typename Iterator> void shellSort(Iterator first, Iterator last)
{
    auto distance = std::distance(first, last);
    if (distance < 2) {
        return;
    }

    auto* arr = first.getArray();

    // 使用经典的希尔排序间隔序列
    for (int gap = distance / 2; gap > 0; gap /= 2) {
        for (auto i = first + gap; i != last; ++i) {
            auto temp = *i;
            auto j    = i;

            while (j >= first + gap && *(j - gap) > temp) {
                arr->highlightElements(j.getIndex(), (j - gap).getIndex()); // 高亮交换的元素
                *j  = *(j - gap);
                j  -= gap;
            }
            *j = temp;
        }
    }
}

/**
 * @brief 基数排序（仅适用于非负整数）
 */
template <typename Iterator> void radixSort(Iterator first, Iterator last)
{
    if (first == last) {
        return;
    }

    // 找到最大值以确定位数
    auto  max_val = *std::max_element(first, last);
    auto* arr     = first.getArray();

    std::vector<int>                                                count(10, 0);
    std::vector<std::pair<Iterator, typename Iterator::value_type>> output;

    // 对每一位进行计数排序
    for (int exp = 1; max_val.getValue() / exp > 0; exp *= 10) {
        MyElement::enableVisualization(false);                            // 禁用可视化以避免干扰
        output.resize(std::distance(first, last), {first, MyElement(0)}); // 确保输出数组大小正确
        count.assign(10, 0);                                              // 重置计数器
        MyElement::enableVisualization(true);                             // 恢复可视化

        // 计算每个数字的频率
        for (auto it = first; it != last; ++it) {
            count[(it->getValue() / exp) % 10]++;
        }

        // 计算累积计数
        for (int i = 1; i < 10; i++) {
            count[i] += count[i - 1];
        }

        // 构建输出数组
        int index = std::distance(first, last) - 1;
        for (auto it = last - 1; it >= first; --it, --index) {
            output[count[(it->getValue() / exp) % 10] - 1] = std::make_pair(it, *it);
            count[(it->getValue() / exp) % 10]--;
        }

        // 将输出数组复制回原数组
        index = 0;
        for (auto it = first; it != last; ++it, ++index) {
            auto& elem = output[index];
            arr->highlightElements(it.getIndex(), elem.first.getIndex()); // 高亮当前元素
            *it = elem.second;
        }
    }
}

/**
 * @brief 计数排序
 */
template <typename Iterator> void countingSort(Iterator first, Iterator last)
{
    if (first == last) {
        return;
    }

    auto* arr = first.getArray();

    // 找到最大值和最小值
    auto min_val = std::min_element(first, last)->getValue();
    auto max_val = std::max_element(first, last)->getValue();

    int range = max_val - min_val + 1;

    std::vector<int> count(range, 0);

    // 计算每个元素的频率
    for (auto it = first; it != last; ++it) {
        int index = it->getValue() - min_val;
        arr->highlightElements(it.getIndex()); // 高亮当前元素
        arr->notifyVisualization();
        count[it->getValue() - min_val]++;
    }

    // 重建数组
    auto current = first;
    for (int i = 0; i < range; i++) {
        while (count[i]-- > 0) {
            *current = typename std::iterator_traits<Iterator>::value_type(i + min_val);
            ++current;
        }
    }
}

// 排序算法信息结构
struct SortAlgorithm
{
    std::string                                                   name;
    std::function<void(MyArray::MyIterator, MyArray::MyIterator)> func;
};

/**
 * @class VisualizationState
 * @brief 管理排序可视化的状态和界面显示
 *
 * 这个类封装了排序可视化演示的所有状态，包括数组、算法列表、
 * 当前算法索引等，并提供相应的操作方法
 */
class VisualizationState
{
public:
    /**
     * @brief 构造函数，初始化可视化状态
     */
    VisualizationState() : m_array(ARRAY_SIZE), m_currentAlgorithm(0), m_sorting(false)
    {
        // 算法列表初始为空，通过addAlgorithm方法动态添加
        m_array.setVisualizer(this);
    }

    /**
     * @brief 添加排序算法
     * @param name 算法名称
     * @param func 算法函数
     */
    void addAlgorithm(const std::string& name, std::function<void(MyArray::MyIterator, MyArray::MyIterator)> func)
    {
        m_algorithms.push_back({name, func});
    }

    /**
     * @brief 绘制可视化界面
     * @param updateScene 是否更新场景
     */
    void updateScene(bool flush = true)
    {
        MyElement::enableVisualization(false); // 禁用统计

        handleKeyMsg();
        cleardevice();

        // 绘制标题和状态信息
        setcolor(WHITE);
        setfont(20, 0, TEXT_FONT_NAME);

        // 主标题
        outtextxy(10, 10, TEXT_WINDOW_TITLE);

        // 状态信息
        setfont(14, 0, TEXT_FONT_NAME);
        setcolor(CYAN);

        xyprintf(10, 40, TEXT_STATS_FORMAT, m_array.getCurrentAlgorithm().c_str(), m_array.getWriteCount(),
            m_array.getCompareCount(), m_array.getReadCount());

        // 显示速度信息
        setcolor(LIGHTGREEN);
        xyprintf(10, 65, TEXT_SPEED_INFO, g_operationDelay);

        // 获取数组数据
        const auto& data = m_array.getData();

        setcolor(BLACK);
        setfont(12, 0, "Arial");

        // 绘制数组元素
        for (int i = 0; i < data.size(); ++i) {
            int x      = i * BAR_WIDTH;
            int height = data[i].getValue(); // 使用 getValue() 获取数值

            // 设置颜色
            if (i == m_array.getHighlightedIndex1()) {
                setfillcolor(RED); // 当前访问的元素
            } else if (i == m_array.getHighlightedIndex2()) {
                setfillcolor(YELLOW); // 第二个比较元素
            } else {
                setfillcolor(LIGHTBLUE); // 普通元素
            }

            // 绘制矩形条
            bar(x, WINDOW_HEIGHT - height - 20, x + BAR_WIDTH - 2, WINDOW_HEIGHT - 20);

            // 可选：绘制数值（只在条足够宽时显示）
            if (BAR_WIDTH > 20) {
                int text_x = x + BAR_WIDTH / 2;
                int text_y = WINDOW_HEIGHT - height / 2 - 10;
                if (text_y > WINDOW_HEIGHT - height - 20 + 5) {
                    xyprintf(text_x - 10, text_y, "%d", data[i].getValue()); // 减去10来大致居中
                }
            }
        }

        // 清除高亮
        // m_array.clearHighlight();

        if (flush) {
            delay_ms(1);
        }

        MyElement::enableVisualization(true);
    }

    /**
     * @brief 显示界面
     */
    void showInterface()
    {
        updateScene(false);

        MyElement::enableVisualization(false);

        // 显示说明
        setcolor(WHITE);
        setfont(16, 0, TEXT_FONT_NAME);
        outtextxy(10, 50, TEXT_CONTROLS_TITLE);
        outtextxy(10, 80, TEXT_CONTROLS_START);
        outtextxy(10, 110, TEXT_CONTROLS_SHUFFLE);
        outtextxy(10, 140, TEXT_CONTROLS_NEXT);
        outtextxy(10, 170, TEXT_CONTROLS_PREV);
        outtextxy(10, 200, TEXT_CONTROLS_AUTO);
        outtextxy(10, 230, TEXT_CONTROLS_SPEED);
        outtextxy(10, 260, TEXT_CONTROLS_EXIT);

        // 显示当前算法（如果有算法的话）
        if (!m_algorithms.empty()) {
            setcolor(YELLOW);
            setfont(18, 0, TEXT_FONT_NAME);
            xyprintf(10, 300, TEXT_CURRENT_ALGORITHM, m_algorithms[m_currentAlgorithm].name.c_str());
        }

        // 显示速度信息
        setcolor(CYAN);
        setfont(14, 0, TEXT_FONT_NAME);
        xyprintf(10, 330, TEXT_SPEED_INFO, g_operationDelay);

        if (m_array.isSorted()) {
            setcolor(GREEN);
            outtextxy(10, 360, TEXT_ARRAY_SORTED);
        } else {
            setcolor(RED);
            outtextxy(10, 360, TEXT_ARRAY_NOT_SORTED);
        }

        delay_fps(60);
        MyElement::enableVisualization(true);
    }

    /**
     * @brief 开始排序当前算法
     */
    void startSorting()
    {
        if (m_sorting || m_algorithms.empty()) {
            return;
        }

        if (m_array.isSorted()) {
            shuffleArray();
        }

        m_sorting = true;

        // 设置当前算法名称并重置统计
        m_array.setCurrentAlgorithm(m_algorithms[m_currentAlgorithm].name);

        cleardevice();
        setcolor(YELLOW);
        setfont(24, 0, TEXT_FONT_NAME);
        xyprintf(10, 10, TEXT_EXECUTING, m_algorithms[m_currentAlgorithm].name.c_str());

        // 显示算法信息
        setcolor(WHITE);
        setfont(14, 0, TEXT_FONT_NAME);
        switch (m_currentAlgorithm) {
        case 0:
            outtextxy(10, 50, TEXT_BUBBLE_COMPLEXITY);
            break;
        case 1:
            outtextxy(10, 50, TEXT_SELECTION_COMPLEXITY);
            break;
        case 2:
            outtextxy(10, 50, TEXT_INSERTION_COMPLEXITY);
            break;
        case 3:
            outtextxy(10, 50, TEXT_QUICK_COMPLEXITY);
            break;
        case 4:
            outtextxy(10, 50, TEXT_MERGE_COMPLEXITY);
            break;
        case 5:
            outtextxy(10, 50, TEXT_HEAP_COMPLEXITY);
            break;
        case 6:
            outtextxy(10, 50, TEXT_SHELL_COMPLEXITY);
            break;
        case 7:
            outtextxy(10, 50, TEXT_RADIX_COMPLEXITY);
            break;
        case 8:
            outtextxy(10, 50, TEXT_COUNTING_COMPLEXITY);
            break;
        case 9:
            outtextxy(10, 50, TEXT_STD_COMPLEXITY);
            break;
        case 10:
            outtextxy(10, 50, TEXT_STD_STABLE_COMPLEXITY);
            break;
        default:
            outtextxy(10, 50, TEXT_STD_COMPLEXITY);
            break;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool success   = false;
        try {
            m_algorithms[m_currentAlgorithm].func(m_array.begin(), m_array.end());
            success = true;
        }
        catch (...) {
        }

        m_sorting = false;

        if (success) {
            auto endTime  = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            setcolor(GREEN);
            setfont(20, 0, TEXT_FONT_NAME);
            outtextxy(10, 80, TEXT_SORT_COMPLETE);

            setcolor(CYAN);
            setfont(16, 0, TEXT_FONT_NAME);
            xyprintf(10, 110, TEXT_SORT_TIME, duration.count());
        } else {
            setcolor(RED);
            setfont(20, 0, TEXT_FONT_NAME);
            outtextxy(10, 80, TEXT_SORTING_INTERRUPTED);
        }

        updateScene();
    }

    /**
     * @brief 打乱数组
     */
    void shuffleArray()
    {
        if (m_sorting) {
            return;
        }

        bool enableVisualization = MyElement::visualizationEnabled();
        MyElement::enableVisualization(false); // 禁用统计以避免干扰
        m_array.shuffle();
        setcolor(CYAN);
        setfont(16, 0, TEXT_FONT_NAME);
        outtextxy(10, 360, TEXT_ARRAY_SHUFFLED);
        MyElement::enableVisualization(enableVisualization); // 恢复统计状态
    }

    /**
     * @brief 切换到下一个算法
     */
    void nextAlgorithm()
    {
        if (m_sorting || m_algorithms.empty()) {
            return;
        }
        m_currentAlgorithm = (m_currentAlgorithm + 1) % m_algorithms.size();
    }

    /**
     * @brief 切换到上一个算法
     */
    void previousAlgorithm()
    {
        if (m_sorting || m_algorithms.empty()) {
            return;
        }
        m_currentAlgorithm = (m_currentAlgorithm - 1 + m_algorithms.size()) % m_algorithms.size();
    }

    /**
     * @brief 自动演示所有算法
     */
    void autoDemo()
    {
        if (m_sorting || m_algorithms.empty()) {
            return;
        }

        m_sorting = true;
        for (int i = 0; i < m_algorithms.size(); ++i) {
            m_array.shuffle();

            // 设置当前算法名称并重置统计
            m_array.setCurrentAlgorithm(m_algorithms[i].name);

            cleardevice();
            setcolor(YELLOW);
            setfont(24, 0, TEXT_FONT_NAME);
            xyprintf(10, 10, TEXT_EXECUTING_BATCH, m_algorithms[i].name.c_str(), i + 1, (int)m_algorithms.size());

            auto startTime = std::chrono::high_resolution_clock::now();
            m_algorithms[i].func(m_array.begin(), m_array.end());
            auto endTime  = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            setcolor(GREEN);
            setfont(20, 0, TEXT_FONT_NAME);
            outtextxy(10, 50, TEXT_SORT_COMPLETE);

            setcolor(CYAN);
            setfont(16, 0, TEXT_FONT_NAME);
            xyprintf(10, 80, TEXT_SORT_TIME_SHORT, duration.count());

            updateScene();

            if (i < m_algorithms.size() - 1) {
                setcolor(WHITE);
                outtextxy(10, 120, TEXT_NEXT_ALGORITHM);
            } else {
                setcolor(LIGHTGREEN);
                setfont(18, 0, TEXT_FONT_NAME);
                outtextxy(10, 120, TEXT_AUTO_DEMO_COMPLETE);
            }
        }
        m_sorting = false;
    }

    /**
     * @brief 处理键盘输入
     * @param ch 输入的字符
     * @return 是否继续运行（false表示退出）
     */
    bool handleInput(char ch)
    {
        switch (ch) {
        case '+':
        case '=':
            g_operationDelay = std::max(0, g_operationDelay - 10); // 减少操作延迟时间（加速动画）
            return true;
        case '-':
        case '_':
            g_operationDelay = std::min(500, g_operationDelay + 10); // 增加操作延迟时间（减速动画）
            return true;
        default:
            break;
        }

        if (m_sorting) {
            if (ch == 27) {                                               // ESC键
                throw std::runtime_error("Sorting interrupted by user."); // 用户中断排序
            }
            return true; // 排序进行中，忽略其他按键输入
        }

        switch (ch) {
        case ' ':  // 空格键
        case '\n': // 回车键
        case '\r': // 回车键
        case 's':  // S键
        case 'S':  // S键（大写）
            startSorting();
            break;

        case 27:  // ESC键 - 退出或重新打乱
        case 'r': // R键
        case 'R': // R键（大写）
            shuffleArray();
            break;

        case 'n':  // N键
        case 'N':  // N键（大写）
        case 0x27: // 右箭头键
        case 0x28: // 下箭头键
            nextAlgorithm();
            break;

        case 'p':  // P键
        case 'P':  // P键（大写）
        case 0x25: // 左箭头键
        case 0x26: // 上箭头键
            previousAlgorithm();
            break;

        case 'a': // A键
        case 'A': // A键（大写）
            autoDemo();
            break;
        }
        return true;
    }

    void handleKeyMsg()
    {
        while (kbhit()) {
            char ch = getch();
            if (!handleInput(ch)) {
                break;
            }
        }
    }

    /**
     * @brief 运行主循环
     */
    void run()
    {
        showInterface();

        while (is_run()) {
            handleKeyMsg();
            showInterface();
        }
    }

private:
    MyArray                    m_array;
    std::vector<SortAlgorithm> m_algorithms;

    int  m_currentAlgorithm;
    bool m_sorting;
};

// 实现 MyArray::notifyVisualization() 方法
void MyArray::notifyVisualization()
{
    if (m_visualizerPtr) {
        m_visualizerPtr->updateScene();
    }
}

// 实现 MyElement::notifyVisualization() 方法
void MyElement::notifyVisualization() const
{
    if (m_visualizerPtr && m_enableVisualization) {
        m_visualizerPtr->updateScene();
    }
}

int main()
{
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, INIT_RENDERMANUAL);
    setbkcolor(BLACK);
    setbkmode(TRANSPARENT);
    setcaption("EGE - " TEXT_WINDOW_TITLE);

    // 创建可视化状态管理器
    VisualizationState visualizer;

    // 添加所有排序算法
    visualizer.addAlgorithm(TEXT_BUBBLE_SORT, bubbleSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_SELECTION_SORT, selectionSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_INSERTION_SORT, insertionSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_QUICK_SORT, quickSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_MERGE_SORT, mergeSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_HEAP_SORT, heapSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_SHELL_SORT, shellSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_RADIX_SORT, radixSort<MyArray::MyIterator>);
    visualizer.addAlgorithm(TEXT_COUNTING_SORT, countingSort<MyArray::MyIterator>);

    visualizer.addAlgorithm(
        TEXT_STD_SORT, [](MyArray::MyIterator first, MyArray::MyIterator last) { std::sort(first, last); });
    visualizer.addAlgorithm(TEXT_STD_STABLE_SORT,
        [](MyArray::MyIterator first, MyArray::MyIterator last) { std::stable_sort(first, last); });

    // 运行可视化演示
    visualizer.run();

    closegraph();
    return 0;
}
