#pragma once

#include <string>
#include <vector>

namespace gpng {

    class Image {
        public:
            uint8_t* image;
            std::vector<uint8_t> main_buffer;

            int width;
            int height;

            Image(int w, int h);

            uint8_t& operator()(int column, int row, int colour);
            void close();
            void output_png(std::string filename);
            void deflate_no_compression(std::vector<uint8_t>& buffer);
    };

}