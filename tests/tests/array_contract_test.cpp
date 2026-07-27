#include "array.h"

#include <cstdlib>
#include <iostream>

namespace {

int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

} // namespace

int main()
{
    ege::Array<int> source;
    for (int value = 0; value < 10; ++value) {
        source.push_back(value);
    }

    ege::Array<int> copied(source);
    copied.push_back(10);
    expect(copied.size() == 11 && copied.front() == 0 && copied.back() == 10,
           "copy construction preserves capacity and supports appending");

    ege::Array<int> assigned;
    assigned.push_back(99);
    assigned = copied;
    expect(assigned.size() == copied.size() &&
               assigned.front() == 0 && assigned.back() == 10,
           "copy assignment replaces owned storage");

    assigned.resize(3);
    expect(assigned.size() == 3 && assigned.front() == 0 &&
               assigned.back() == 2,
           "shrinking retains only elements inside the new capacity");

    assigned.resize(0);
    expect(assigned.size() == 0,
           "resizing to zero clears the logical contents");
    assigned.push_back(42);
    expect(assigned.size() == 1 && assigned.front() == 42,
           "an array can grow again after resizing to zero");

    if (failures != 0) {
        std::cerr << failures << " array contract assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All array contract assertions passed\n";
    return EXIT_SUCCESS;
}
