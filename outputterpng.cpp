// Greg's PNG exporter library.
// CRC generator was taken from https://www.w3.org/TR/PNG-CRCAppendix.html

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// CRC Generator
/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void) {
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

unsigned long update_crc(unsigned long crc, uint8_t* buf, int len)
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
uint32_t get_crc(uint8_t* buf, int len)
{
    return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

// Outputs all bytes of a given buffer (for example main_buffer) to the console
template <typename T>
void print_buffer(std::vector<uint8_t>& buffer, T val_ptr, size_t length, bool reverse = true) {
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
void write_list(std::ofstream& image, const uint8_t* val, size_t length) {
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
void push_to_buffer(std::vector<uint8_t>& buffer, T val_ptr, size_t length, bool reverse = true) {
    uint8_t* start = reinterpret_cast<uint8_t*>(val_ptr);

    for (int i = 0; i < length; ++i) {
        if (reverse == true) {
            buffer.push_back(*(start + length - 1 - i));
        } else {
            buffer.push_back(*(start + i));
        }
    }
}

int main() {
    std::ofstream image;
    // Open for writing in binary
    image.open("img2.png", std::ios::out | std::ios::binary);

    // Width and height of image in pixels
    uint32_t width = 1;
    uint32_t height = 1;

    if (image.is_open()) {

        // The PNG file starts with a header, which is then followed by multiple chunks:
        // IHDR, containing image's width, height, bit depth, colour type, compression method, filter method and interlace method
        // IDAT, which contains the image's data (there may be multiple of these)
        // IEND, identifying the end of the file. Its data section is empty.

        // Each chunk has 4 sections:
        // 4 bytes specifying the length of the chunk data in bytes, n
        // 4 bytes identifying the chunk type (e.g. IHDR or PNG header)
        // n bytes for the chunk data
        // 4 bytes for the CRC, a parity check, which uses the bytes in the chunk type and chunk data sections

        // Every byte that will be output to the png file is first "pushed" into this main buffer array. Then is it all written at once using write_list().
        std::vector<uint8_t> main_buffer;

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
        // push chunk length to main buffer
        uint32_t size = 0x0c;
        push_to_buffer(main_buffer, &size, sizeof(size));

        // chunk type and chunk data
        // once again stored (in idat_dat) for CRC calculation later
        std::vector<uint8_t> idat_dat;
        
        // chunk type and other specifiers
        idat_dat.insert(idat_dat.end(), {'I', 'D', 'A', 'T', 0x08, 0xd7});
        // DEFLATE block (from wikipedia)
        idat_dat.insert(idat_dat.end(), {0x63, 0xf8, 0xcf, 0xc0, 0x00, 0x00});
        // ZLIB check value (from wikipedia)
        idat_dat.insert(idat_dat.end(), {0x03, 0x01, 0x01, 0x00});

        // push IDAT chunk type and chunk data to main buffer
        push_to_buffer(main_buffer, &idat_dat[0], idat_dat.size(), false);

        // calculate CRC for IDAT and push to main buffer
        crc = get_crc(&idat_dat[0], idat_dat.size());
        push_to_buffer(main_buffer, &crc, sizeof(crc));

        // IEND Chunk (end of png file)
        main_buffer.insert(main_buffer.end(), {0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82});

        // send all to file
        write_list(image, &main_buffer[0], main_buffer.size());

    }

    image.close();
}