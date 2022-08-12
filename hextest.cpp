#include <iostream>
#include <vector>
#include <fstream>

void messvector(std::vector<int> inp) {
    inp[2] = 12;
}

int main() {
    // int8_t hexval = 0x50;
    // std::cout << std::hex << static_cast<int>(hexval);
    // std::vector<int> a = {1,2,3,4};
    // messvector(a);
    // std::cout << a[2];

    std::vector<int> arr1 = {1,2,3,4,5};
    int* arrptr = &arr1[0];
    std::cout << arrptr[4];
}