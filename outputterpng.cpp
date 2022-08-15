// Greg's PNG exporter library.
// CRC generator was taken from https://www.w3.org/TR/PNG-CRCAppendix.html
// Adler generator was taken from https://en.wikipedia.org/wiki/Adler-32

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

// ADLER-32 Generator
const uint32_t MOD_ADLER = 65521;
uint32_t adler32(uint8_t *data, size_t len) 
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

// Class to store an image's pixel data in. Access pixel by doing Image(row, column, colour) where colours are R:0, G:1, B:2
class Image {
    private:
        uint8_t* image;

    public:
        int width;
        int height;

        Image(int w, int h) {
            image = new uint8_t[w * h * 3];
            width = w;
            height = h;
        }

        uint8_t& operator()(int row, int column, int colour) {
            return image[row * width * 3 + column * 3 + colour];
        }

        void close() {
            delete[] image;
        }
};

// Class to allow working with data bit by bit. when push() is called, the input bit is put in the next bit available, from MSB to LSB (left to right).
class Bitstream {
    private:
        uint8_t byte = 0;
        int bit_no = 7;
    public:
        std::vector<uint8_t>& buffer;

        Bitstream(std::vector<uint8_t>& buffer_ref) : buffer(buffer_ref) {}

        void push(bool bit) {
            byte |= ((bit ? 1 : 0) << bit_no);
            // std::cerr << "bit_no " << bit_no << "\n";
            --bit_no;

            if (bit_no == -1) {
                buffer.push_back(byte);
                byte = 0;
                bit_no = 7;
                // std::cerr << "Byte pushed!\n";
            }
        }

        void print_byte() {
            std::cerr << std::hex << static_cast<int>(byte) << std::dec;
        }

        template <typename T>
        // for deflate, numbers must be pushed in reverse bit order
        void push_number(T number_ptr, int no_of_bits) {
            // std::cerr << "entering\n";
            uint8_t* start = reinterpret_cast<uint8_t*>(number_ptr);
            int no_of_bytes = (no_of_bits + 8 - 1) / 8; // integer division rounding up
            int first_bit_pos = no_of_bits % 8; // number of bits in most significant byte
            if (first_bit_pos == 0) { first_bit_pos = 8; }

            uint8_t byte;
            // iterates through bytes from lsbyte upwards, but leaves out msbyte (remember that byte sequences are stored as little-endian on most systems)
            for (int i = 0; i < no_of_bytes - 1; ++i) {
                byte = *(start + i);
                // iterates through a byte, from lsbit to msbit, pushing each bit with Bitstream::push() method
                for (int j = 0; j < 8; ++j) {
                    // std::cerr << "Pushing bit\n";
                    uint8_t to_push = byte & (0x01 << j);
                    push(to_push);
                }
            }
            // pushes bits in msbyte
            byte = *(start + no_of_bytes - 1);
            for (int i = 0; i < first_bit_pos; ++i) {
                push(byte & (0x01 << i));
                // std::cerr << "Pushing msb bit\n";
            }
        }
};

// Outputs all bytes of a given buffer (for example main_buffer) to the console
template <typename T>
void print_buffer(T val_ptr, size_t length, bool reverse = true) {
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

void deflate_no_compression(const Image& img, std::vector<uint8_t>& buffer, uint8_t* start, size_t length) {
    // This is where numbers that need to be bit-flipped need to be pushed, as well as anything else that needs to be pushed as individual bits
    Bitstream stream(buffer);

    // header (1st byte: IS_LAST, bytes 2&3: compression method 00 (uncompressed), rest is byte padding which is required when there is no compression)
    buffer.push_back(0b10000000);

    uint16_t len = static_cast<uint16_t>(length);
    // 16-bit number that gives the length of the coming bitstream
    stream.push_number(&len, 16);
    // bit-flip of above
    len = ~len;
    stream.push_number(&len, 16);

    // image data
    // 0x00 to specifyy no filter, one white pixel, one black pixel
    std::vector<uint8_t> image_dat = {0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00};
    for (int i = 0; i < image_dat.size(); ++i) {
        stream.push_number(&image_dat[i], 8);
    }

}

int main() {
    std::ofstream img;
    // Open for writing in binary
    img.open("img2.png", std::ios::out | std::ios::binary);

    // Width and height of image in pixels
    uint32_t width = 2;
    uint32_t height = 2;

    if (img.is_open()) {

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

        // chunk type and chunk data
        // once again stored (in idat_dat) for CRC calculation later
        std::vector<uint8_t> idat_dat;
        
        // chunk type and other specifiers

        // FROM WIKI
        // idat_dat.insert(idat_dat.end(), {'I', 'D', 'A', 'T', 0x08, 0xd7});
        // DEFLATE block (from wikipedia)
        // idat_dat.insert(idat_dat.end(), {0x63, 0xf8, 0xcf, 0xc0, 0x00, 0x00});
        // ZLIB check value (from wikipedia)
        // idat_dat.insert(idat_dat.end(), {0x03, 0x01, 0x01, 0x00});

        // my own test image
        idat_dat.insert(idat_dat.end(), {'I', 'D', 'A', 'T', 0x08, 0xd7}); // FOR my test image, specifying no DEFLATE compression
        uint8_t dummy = 0;
        Image dummy_img = Image(1,1);
        deflate_no_compression(dummy_img, idat_dat, &dummy, 14);
        // ADLER-32
        std::vector<uint8_t> image_dat = {0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00};
        uint32_t adler = adler32(&image_dat[0], 14);
        push_to_buffer(idat_dat, &adler, sizeof(adler));

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
        write_list(img, &main_buffer[0], main_buffer.size());

        // TESTS for Image class
        // Image testImage{100, 100};
        // testImage(99,99,2) = 3;
        // std::cout << static_cast<int>(testImage(99,99,2)) << "\n";
        // std::cout << testImage.width << " " << testImage.height;

        // TESTS for Bitstream class
        // std::vector<uint8_t> test_buffer;
        // uint16_t test_no = 0b0000011011101011;
        // Bitstream test_stream(test_buffer);
        // test_stream.push_number(&test_no, 13);
        // std::cerr << "Printing buffer\n";
        // print_buffer(&test_buffer[0], test_buffer.size());
        // std::cerr << "Done printing buffer\n";
        // test_stream.print_byte();

    }

    img.close();
}