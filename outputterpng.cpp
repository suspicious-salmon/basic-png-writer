// Greg's PNG exporter library.
// CRC generator was taken from https://www.w3.org/TR/PNG-CRCAppendix.html

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

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
    image.open("img2.png", std::ios::out | std::ios::binary);

    uint32_t width = 1;
    uint32_t height = 1;

    if (image.is_open()) {

        // Every byte that will be output to the png file is first "pushed" into this buffer array. Then is it all written at once using write_list().
        std::vector<uint8_t> main_buffer;

        // PNG header
        main_buffer.insert(main_buffer.end(), {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'});

        // this works, but is difficult to implement for the whole program. Instead of image.write I use my own function, write_list().
        // uint8_t buffer[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
        // image.write(reinterpret_cast<char*>(buffer), 8);

        // First section of IHDR pushed straight to buffer
        main_buffer.insert(main_buffer.end(), {0x0, 0x0, 0x0, 0x0D});

        // Second section of IHDR, including width and height, pushed to ihdr_dat to allow for crc calculation later
        std::vector<uint8_t> ihdr_dat; // stores data block of ihdr

        ihdr_dat.insert(ihdr_dat.end(), {'I', 'H', 'D', 'R'});

        push_to_buffer(ihdr_dat, &width, sizeof(width));
        push_to_buffer(ihdr_dat, &height, sizeof(height));

        // Third section of IHDR, pushed to ihdr_dat to allow for crc calculation later
        ihdr_dat.insert(ihdr_dat.end(), {0x08, 0x02, 0x00, 0x00, 0x00});

        // Push IHDR to buffer
        push_to_buffer(main_buffer, &ihdr_dat[0], ihdr_dat.size(), false);

        // CRC for IHDR & push to buffer
        uint32_t crc = get_crc(&ihdr_dat[0], ihdr_dat.size());
        push_to_buffer(main_buffer, &crc, sizeof(crc));

        // PNG image data (IDAT chunk)

        // size of chunk
        uint32_t size = 0x0c;
        push_to_buffer(main_buffer, &size, sizeof(size));

        // stores data for CRC calculation
        std::vector<uint8_t> idat_dat;
        
        // identifier and other specifiers
        idat_dat.insert(idat_dat.end(), {'I', 'D', 'A', 'T', 0x08, 0xd7});

        // DEFLATE block (from wikipedia)
        idat_dat.insert(idat_dat.end(), {0x63, 0xf8, 0xcf, 0xc0, 0x00, 0x00});
        // ZLIB check value (from wikipedia)
        idat_dat.insert(idat_dat.end(), {0x03, 0x01, 0x01, 0x00});

        // push IDAT to buffer
        push_to_buffer(main_buffer, &idat_dat[0], idat_dat.size(), false);

        // CRC for IDAT & push to buffer
        crc = get_crc(&idat_dat[0], idat_dat.size());
        push_to_buffer(main_buffer, &crc, sizeof(crc));

        // IEND (end of png file)
        main_buffer.insert(main_buffer.end(), {0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82});

        // send all to file
        write_list(image, &main_buffer[0], main_buffer.size());

    }

    image.close();
}