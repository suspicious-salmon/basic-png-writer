// Greg's PNG exporter library.
// CRC generator was taken from https://www.w3.org/TR/PNG-CRCAppendix.html
// Adler generator was taken from https://en.wikipedia.org/wiki/Adler-32

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "gpng.h"

// CRC Generator
/* Table of CRCs of all 8-bit messages. */
static unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
static int crc_table_computed = 0;

/* Make the table for a fast CRC. */
static void make_crc_table(void) {
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
    c = (unsigned long) n;
    for (k = 0; k < 8; k++) {
        if (c & 1)
        c = 0xedb88320L ^ (c >> 1);
        else
        c = c >> 1;
    }
    crc_table[n] = c;
    }
    crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
    should be initialized to all 1's, and the transmitted value
    is the 1's complement of the final running CRC (see the
    crc() routine below)). */

static unsigned long update_crc(unsigned long crc, uint8_t* buf, int len)
{
    unsigned long c = crc;
    int n;

    if (!crc_table_computed)
    make_crc_table();
    for (n = 0; n < len; n++) {
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
    }
    return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
static uint32_t get_crc(uint8_t* buf, int len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

// ADLER-32 Generator
static const uint32_t MOD_ADLER = 65521;
static uint32_t adler32(uint8_t *data, size_t len) 
/* 
    where data is the location of the data in physical memory and 
    len is the length of the data in bytes 
*/
{
    uint32_t a = 1, b = 0;
    size_t index;
    
    // Process each byte of the data in order
    for (index = 0; index < len; ++index)
    {
        a = (a + data[index]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    
    return (b << 16) | a;
}

// Outputs all bytes of a given buffer (for example main_buffer) to the console
template <typename T>
static void print_buffer(T val_ptr, size_t length, bool reverse = true) {
    uint8_t* start = reinterpret_cast<uint8_t*>(val_ptr);

    for (int i = 0; i < length; ++i) {
        if (reverse == true) {
            std::cout << std::hex << static_cast<int>(*(start + length - 1 - i)) << std::dec << " ";
        } else {
            std::cout << std::hex <<  static_cast<int>(*(start + i)) << std::dec << " ";
        }
    }
    std::cout << "\n";
}

// Writes a vector array of bytes into a given file
static void write_list(std::ofstream& image, const uint8_t* val, size_t length) {
    for (int i = 0; i < length; ++i) {
        image << static_cast<unsigned char>(*(val + i));
    }
}

// VERSION 1 of push_to_buffer, requires reinterpret_casting input start pointer to uint8_t* for every function call
// void push_to_buffer(std::vector<uint8_t>& buffer, uint8_t* start, size_t length) {
//     for (int i = 0; i < length; ++i) {
//         buffer.push_back(*(start + length - 1 - i));
//     }
// }

// VERSION 2 of push_to_buffer, accepts multiple different sizes of pointer (should all be uint<something>_t)
// first argument is the buffer you are pushing TO
// second argument specifies start mem location of what you are pushing
// reverse = true for big-endian output, as png requires
template <typename T>
static void push_to_buffer(std::vector<uint8_t>& buffer, T val_ptr, size_t length, bool reverse = true) {
    uint8_t* start = reinterpret_cast<uint8_t*>(val_ptr);

    for (int i = 0; i < length; ++i) {
        if (reverse == true) {
            buffer.push_back(*(start + length - 1 - i));
        } else {
            buffer.push_back(*(start + i));
        }
    }
}

// Returns the value that has to be added to the number to make it a multiple of 31. Used for FCHECK bits in FLG byte (see zlib standard).
static uint8_t sum_to_31(int number) {
    int ret = 31 - (number % 31);
    if (ret == 31) { return 0; }
    return ret;
}

namespace gpng {

    Image::Image(int w, int h) {
        image = new uint8_t[w * h * 3];
        width = w;
        height = h;
    }

    uint8_t& Image::operator()(int column, int row, int colour) {
        return image[row * width * 3 + column * 3 + colour];
    }

    void Image::close() {
        delete[] image;
    }

    void Image::output_png(std::string filename) {
        std::ofstream image_write;
        image_write.open(filename, std::ios::out | std::ios::binary);

        if (image_write.is_open()) {
            // The PNG file starts with a header, which is then followed by multiple chunks:
            // IHDR, containing image's width, height, bit depth, colour type, compression method, filter method and interlace method
            // IDAT, which contains the image's data (there may be multiple of these)
            // IEND, identifying the end of the file. Its data section is empty.

            // Each chunk has 4 sections:
            // 4 bytes specifying the length of the chunk data in bytes, n
            // 4 bytes identifying the chunk type (e.g. IHDR or PNG header)
            // n bytes for the chunk data
            // 4 bytes for the CRC, a parity check, which uses the bytes in the chunk type and chunk data sections

            // PNG Header
            main_buffer.insert(main_buffer.end(), {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'});

            // this works too, but is difficult to implement for the whole program. Instead of image.write I use my own function, write_list().
            // uint8_t buffer[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
            // image.write(reinterpret_cast<char*>(buffer), 8);

            // IHDR Chunk
            // push chunk length to main buffer
            main_buffer.insert(main_buffer.end(), {0x0, 0x0, 0x0, 0x0D});

            // chunk type and chunk data
            // these sections are stored in ihdr_dat to be used for CRC calculation later
            std::vector<uint8_t> ihdr_dat;

            ihdr_dat.insert(ihdr_dat.end(), {'I', 'H', 'D', 'R'});

            push_to_buffer(ihdr_dat, &width, sizeof(width));
            push_to_buffer(ihdr_dat, &height, sizeof(height));

            ihdr_dat.insert(ihdr_dat.end(), {0x08, 0x02, 0x00, 0x00, 0x00});

            // push IHDR chunk type and chunk data to main buffer
            push_to_buffer(main_buffer, &ihdr_dat[0], ihdr_dat.size(), false);

            // calculate CRC for IHDR and push to main buffer
            uint32_t crc = get_crc(&ihdr_dat[0], ihdr_dat.size());
            push_to_buffer(main_buffer, &crc, sizeof(crc));

            // IDAT Chunk

            // chunk type and chunk data
            // once again stored (in idat_dat) for CRC calculation later
            std::vector<uint8_t> idat_dat;

            // my own test image
            idat_dat.insert(idat_dat.end(), {'I', 'D', 'A', 'T'}); // FOR my test image
            uint8_t CMF = 0x78; // max window size of 32k (7) and deflate compression method (8)
            uint8_t FLG = 0b11000000; // (zlib) first 2 bits indicate compression level 3, 3rd bit indicates no dictionary used
            FLG += sum_to_31(static_cast<int>(CMF)*256 + static_cast<int>(FLG));
            idat_dat.push_back(CMF);
            idat_dat.push_back(FLG);

            (*this).deflate_no_compression(idat_dat);

            // push chunk length to main buffer
            uint32_t size = idat_dat.size() - 4;
            push_to_buffer(main_buffer, &size, sizeof(size));

            // push IDAT chunk type and chunk data to main buffer
            push_to_buffer(main_buffer, &idat_dat[0], idat_dat.size(), false);

            // calculate CRC for IDAT and push to main buffer
            crc = get_crc(&idat_dat[0], idat_dat.size());
            push_to_buffer(main_buffer, &crc, sizeof(crc));

            // IEND Chunk (end of png file)
            main_buffer.insert(main_buffer.end(), {0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82});

            // send all to file
            write_list(image_write, &main_buffer[0], main_buffer.size());

            std::cerr << "Writing!\n";
            image_write.close();

        } else {
            std::cerr << "Error in Image::output_png(): could not open image\n";
        }
    }

    void Image::deflate_no_compression(std::vector<uint8_t>& buffer) {
        uint8_t filter_type = 0x00;
        std::vector<uint8_t> uncompressed_buffer; // stores image along with filter types for use in adler32
        for (int y = 0; y < height; ++y) {
            uncompressed_buffer.push_back(filter_type);
            for (int x = 0; x < width; ++x) {
                uncompressed_buffer.push_back( (*this)(x, y, 0) );
                uncompressed_buffer.push_back( (*this)(x, y, 1) );
                uncompressed_buffer.push_back( (*this)(x, y, 2) );
            }
        }
        uint32_t adler = adler32(&uncompressed_buffer[0], uncompressed_buffer.size());

        const int block_size_limit = 32768;
        int raw_length = height * (width * 3 + 1);
        int no_of_blocks = (raw_length + block_size_limit - 5 - 1) / (block_size_limit - 5); // integer division rounding up
        int final_block_raw_length = raw_length % (block_size_limit - 5);
        if (final_block_raw_length == 0) { final_block_raw_length = block_size_limit - 5; }

        int image_pos = 0;
        uint16_t push_length;
        // pushes all blocks but final block
        for (int i = 0; i < no_of_blocks - 1; ++i) {
            std::cerr << "Starting block " << i << "\n";
            buffer.push_back(0x00);
            push_length = block_size_limit - 5;
            push_to_buffer(buffer, &push_length, sizeof(push_length), false);
            push_length = ~push_length;
            push_to_buffer(buffer, &push_length, sizeof(push_length), false);

            for (int j = 0; j < block_size_limit - 5; ++j) {
                buffer.push_back(uncompressed_buffer[image_pos]);
                ++image_pos;
            }
        }

        // pushes final block
        std::cerr << "Starting block " << no_of_blocks - 1 << " (final block)\n";
        buffer.push_back(0x80);
        push_length = final_block_raw_length;
        push_to_buffer(buffer, &push_length, sizeof(push_length), false);
        push_length = ~push_length;
        push_to_buffer(buffer, &push_length, sizeof(push_length), false);

        for (int j = 0; j < final_block_raw_length; ++j) {
            buffer.push_back(uncompressed_buffer[image_pos]);
            ++image_pos;
        }

        // pushes adler32 checksum
        push_to_buffer(buffer, &adler, sizeof(adler));
    }
}