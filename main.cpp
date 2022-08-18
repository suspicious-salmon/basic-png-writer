#include <iostream>
#include <cassert>
#include "gpng.h"

int main() {
    // test image pixels
    int width = 1920;
    int height = 1080;
    assert(width <= 65535 && height <= 65535 && "width, height must each be 65535 or less");
    gpng::Image test_img = gpng::Image(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            test_img(x, y, 0) = 0;
            test_img(x, y, 1) = 255 - x - y;
            test_img(x, y, 2) = 0;
        }
    }

    test_img.output_png("example_img.png");
    // Call this below to deallocate memory (cannot use the image afterwards!)
    test_img.close();

}