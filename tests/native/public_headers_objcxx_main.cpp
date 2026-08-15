#include <iostream>

extern "C" bool egeTestEGEFirstHeadersCompile();
extern "C" bool egeTestAppKitFirstHeadersCompile();

int main()
{
    if (!egeTestEGEFirstHeadersCompile()
        || !egeTestAppKitFirstHeadersCompile()) {
        std::cerr << "Objective-C++ public-header include-order contract failed\n";
        return 1;
    }
    return 0;
}
