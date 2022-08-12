#include <iostream>
#include <fstream>

int main() {
    std::ofstream image;
    image.open("img1.ppm");

    int width = 250;
    int height = 250;

    if (image.is_open()) {

        // metadata
        image << "P3\n" << width << " " << height << "\n255\n";

        // output pixels
        for (int y = 0; y < 250; ++y) {
            for (int x = 0; x < 250; ++x) {
                image << x << " " << x << " " << x << "\n";
            }
        }

    }
}