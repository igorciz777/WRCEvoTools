#include <iostream>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>

// debug switch
const bool DEBUG = true;

const unsigned char EVO9_MAGIC[4] = {'E', 'V', 'O', '9'};      // EVO9 magic number
const unsigned char EVOA_MAGIC[4] = {'E', 'V', 'O', 'A'};      // EVOA magic number
const unsigned char EVOB_MAGIC[4] = {'E', 'V', 'O', 'B'};      // EVOB magic number
const unsigned char BLOCK_MAGIC[4] = {0x52, 0x41, 0x00, 0x03}; // Block magic number
const uint32_t BLOCK_TAG_RAW0 = 0x30574152;                    // 'RAW0' LE

enum class DECOMP
{
    CONTINUE = 0,
    END = 1,
    ERROR = 2
};

typedef struct
{
    uint32_t magic;
    uint32_t unpacked_size;
    uint32_t packed_size;
} evo_dat_header_t;

typedef struct
{
    uint32_t magic;
    uint32_t unpacked_size;
    uint32_t packed_size;
    uint32_t block_tag; // 4th dword present in PS2 block header (total 0x10 bytes)
} evo_block_header_t;

uint8_t scratch_bintree_size[1024];
uint8_t scratch_bintree_pos[1024];
uint8_t scratch_bintree_raw[1024];
static uint32_t g_block_remaining = 0;

/**
 * Checks if there is a next valid block to process and sets bytestream limit
 */
DECOMP Open(std::ifstream &file, evo_block_header_t &block_header)
{
    if (!file.read(reinterpret_cast<char *>(&block_header), sizeof(evo_block_header_t)))
    {
        if (file.eof())
        {
            if (DEBUG)
                std::cout << "No more block headers (EOF)." << std::endl;
            return DECOMP::END;
        }

        std::cerr << "Failed to read block header (I/O error)." << std::endl;
        return DECOMP::ERROR;
    }

    if (memcmp(reinterpret_cast<const void *>(&block_header.magic), BLOCK_MAGIC, 4) != 0)
    {
        // Some files may have trailer/padding after the last block.
        if (block_header.magic == 0 || block_header.magic == 0x00000800 || block_header.magic == 0xFFFFFFFF)
        {
            if (DEBUG)
            {
                std::cout << "Non-block trailer/padding marker 0x" << std::hex << block_header.magic << std::dec
                          << " encountered; treating as end of stream." << std::endl;
            }
            return DECOMP::END;
        }

        std::cerr << "Invalid block magic (not EOF): 0x" << std::hex << block_header.magic << std::dec << std::endl;
        return DECOMP::ERROR;
    }

    if (DEBUG)
    {
        std::cout << "Block magic: " << std::string(reinterpret_cast<char *>(&block_header.magic), 4) << std::endl;
        std::cout << "Unpacked size: " << block_header.unpacked_size << std::endl;
        std::cout << "Packed size: " << block_header.packed_size << std::endl;
        std::cout << "Block tag: 0x" << std::hex << block_header.block_tag << std::dec << std::endl
                  << std::endl;
    }
    // Keep stream positioned right after block header, so decoder starts on payload.
    // ReadBlock must not cross this block boundary.
    g_block_remaining = block_header.packed_size;
    return DECOMP::CONTINUE;
}

uint32_t ReadBlock(std::ifstream &file, std::vector<char> &buffer, unsigned char *&fs_ptr, unsigned char *&out_ptr)
{
    (void)buffer;
    static std::vector<unsigned char> stream_chunk(0x10000);

    uint32_t offset_size = g_block_remaining;
    if (offset_size > stream_chunk.size())
    {
        offset_size = static_cast<uint32_t>(stream_chunk.size());
    }

    fs_ptr = stream_chunk.data();
    out_ptr = fs_ptr + offset_size;

    if (offset_size == 0)
    {
        return 0;
    }

    if (file.read(reinterpret_cast<char *>(fs_ptr), offset_size))
    {
        g_block_remaining -= offset_size;
        return offset_size;
    }
    else
    {
        std::cerr << "Failed to read block from file." << std::endl;
        return 0;
    }
}

void Unpack(std::vector<char> &buffer, std::ifstream &file)
{
    unsigned char current_byte_value;
    int decoded_val;
    unsigned char decoded_output_byte;
    uint32_t temp_bit_value;
    int bits_to_read;
    int table_idx;
    int block_size;
    uint32_t bit_cache;
    uint32_t bit_shift_state_1;
    uint32_t bit_shift_state_2;
    uint32_t bit_shift_state_3;
    uint32_t bit_shift_state_4;
    uint32_t bit_shift_state_5;
    uint32_t bit_shift_state_6;
    uint32_t bit_shift_state_7;
    uint32_t num_blocks;
    unsigned char *in_end;
    unsigned char *in_ptr;
    unsigned char high_byte_value;
    unsigned char next_byte_value;
    unsigned char *temp_read_ptr;
    size_t buffer_index = 0;

    bit_cache = 1;
    in_ptr = 0x0;
    in_end = 0x0;
    while (true)
    {
        /* build bintree_size */
        bit_shift_state_7 = bit_cache >> 1;
        if (bit_shift_state_7 == 0)
        {
            if (in_ptr < in_end)
            {
                high_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                high_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_cache = (uint32_t)high_byte_value;
            bit_shift_state_7 = high_byte_value >> 1 | 0x80;
        }
        if ((bit_cache & 1) == 0)
            break;
        if (in_ptr < in_end)
        {
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        else
        {
            ReadBlock(file, buffer, in_ptr, in_end);
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        in_ptr = temp_read_ptr + 1;
        if (in_ptr < in_end)
        {
            next_byte_value = *in_ptr;
            in_ptr = temp_read_ptr + 2;
        }
        else
        {
            ReadBlock(file, buffer, in_ptr, in_end);
            next_byte_value = *in_ptr;
            in_ptr = in_ptr + 1;
        }
        num_blocks = (uint32_t)current_byte_value + (uint32_t)next_byte_value * 0x100;
        if (in_ptr < in_end)
        {
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        else
        {
            ReadBlock(file, buffer, in_ptr, in_end);
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        in_ptr = temp_read_ptr + 1;
        bit_cache = bit_shift_state_7 >> 1;
        block_size = (uint32_t)current_byte_value << 1;
        if (bit_cache == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
                in_ptr = temp_read_ptr + 2;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
                in_ptr = in_ptr + 1;
            }
            bit_shift_state_7 = (uint32_t)current_byte_value;
            bit_cache = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_1 = bit_cache >> 1;
        if (bit_shift_state_1 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_cache = (uint32_t)current_byte_value;
            bit_shift_state_1 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_2 = bit_shift_state_1 >> 1;
        if (bit_shift_state_2 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_1 = (uint32_t)current_byte_value;
            bit_shift_state_2 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_3 = bit_shift_state_2 >> 1;
        if (bit_shift_state_3 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_2 = (uint32_t)current_byte_value;
            bit_shift_state_3 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_4 = bit_shift_state_3 >> 1;
        if (bit_shift_state_4 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_3 = (uint32_t)current_byte_value;
            bit_shift_state_4 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_5 = bit_shift_state_4 >> 1;
        if (bit_shift_state_5 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_4 = (uint32_t)current_byte_value;
            bit_shift_state_5 = current_byte_value >> 1 | 0x80;
        }
        table_idx = 0;
        do
        {
            bit_shift_state_6 = bit_shift_state_5 >> 1;
            if (bit_shift_state_6 == 0)
            {
                if (in_ptr < in_end)
                {
                    current_byte_value = *in_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, in_ptr, in_end);
                    current_byte_value = *in_ptr;
                }
                in_ptr = in_ptr + 1;
                bit_shift_state_5 = (uint32_t)current_byte_value;
                bit_shift_state_6 = current_byte_value >> 1 | 0x80;
            }
            decoded_val = 0;
            bits_to_read = ((bit_shift_state_7 & 1) * 2 + (bit_cache & 1)) * 2 + (bit_shift_state_1 & 1) + 1;
            if ((bit_shift_state_5 & 1) == 0)
            {
                do
                {
                    bit_shift_state_5 = bit_shift_state_6 >> 1;
                    if (bit_shift_state_5 == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = (uint32_t)current_byte_value;
                        bit_shift_state_5 = current_byte_value >> 1 | 0x80;
                    }
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (bit_shift_state_6 & 1);
                    bit_shift_state_6 = bit_shift_state_5;
                } while (bits_to_read != 0);
                scratch_bintree_size[table_idx] = (uint8_t)decoded_val;
                scratch_bintree_size[table_idx + 0x200] = 0;
            }
            else
            {
                decoded_val = 0;
                scratch_bintree_size[table_idx] = 0;
                bit_shift_state_5 = bit_shift_state_6;
                bits_to_read = ((bit_shift_state_2 & 1) * 2 + (bit_shift_state_3 & 1)) * 2 + (bit_shift_state_4 & 1) + 1;
                do
                {
                    bit_shift_state_6 = bit_shift_state_5 >> 1;
                    temp_bit_value = bit_shift_state_5;
                    if (bit_shift_state_5 >> 1 == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = current_byte_value >> 1 | 0x80;
                        temp_bit_value = (uint32_t)current_byte_value;
                    }
                    bit_shift_state_5 = bit_shift_state_6;
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (temp_bit_value & 1);
                } while (bits_to_read != 0);
                scratch_bintree_size[table_idx + 0x200] = (uint8_t)decoded_val;
            }
            block_size = block_size + -1;
            table_idx = table_idx + 1;
        } while (block_size != 0);
        /* build bintree_raw */
        if (in_ptr < in_end)
        {
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        else
        {
            ReadBlock(file, buffer, in_ptr, in_end);
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        in_ptr = temp_read_ptr + 1;
        bit_cache = bit_shift_state_5 >> 1;
        block_size = (uint32_t)current_byte_value << 1;
        if (bit_cache == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
                in_ptr = temp_read_ptr + 2;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
                in_ptr = in_ptr + 1;
            }
            bit_shift_state_5 = (uint32_t)current_byte_value;
            bit_cache = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_7 = bit_cache >> 1;
        if (bit_shift_state_7 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_cache = (uint32_t)current_byte_value;
            bit_shift_state_7 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_1 = bit_shift_state_7 >> 1;
        if (bit_shift_state_1 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_7 = (uint32_t)current_byte_value;
            bit_shift_state_1 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_2 = bit_shift_state_1 >> 1;
        if (bit_shift_state_2 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_1 = (uint32_t)current_byte_value;
            bit_shift_state_2 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_3 = bit_shift_state_2 >> 1;
        if (bit_shift_state_3 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_2 = (uint32_t)current_byte_value;
            bit_shift_state_3 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_4 = bit_shift_state_3 >> 1;
        if (bit_shift_state_4 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_3 = (uint32_t)current_byte_value;
            bit_shift_state_4 = current_byte_value >> 1 | 0x80;
        }
        table_idx = 0;
        do
        {
            bit_shift_state_6 = bit_shift_state_4 >> 1;
            if (bit_shift_state_6 == 0)
            {
                if (in_ptr < in_end)
                {
                    current_byte_value = *in_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, in_ptr, in_end);
                    current_byte_value = *in_ptr;
                }
                in_ptr = in_ptr + 1;
                bit_shift_state_4 = (uint32_t)current_byte_value;
                bit_shift_state_6 = current_byte_value >> 1 | 0x80;
            }
            decoded_val = 0;
            bits_to_read = ((bit_shift_state_5 & 1) * 2 + (bit_cache & 1)) * 2 + (bit_shift_state_7 & 1) + 1;
            if ((bit_shift_state_4 & 1) == 0)
            {
                do
                {
                    bit_shift_state_4 = bit_shift_state_6 >> 1;
                    if (bit_shift_state_4 == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = (uint32_t)current_byte_value;
                        bit_shift_state_4 = current_byte_value >> 1 | 0x80;
                    }
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (bit_shift_state_6 & 1);
                    bit_shift_state_6 = bit_shift_state_4;
                } while (bits_to_read != 0);
                scratch_bintree_raw[table_idx] = (uint8_t)decoded_val;
                scratch_bintree_raw[table_idx + 0x200] = 0;
            }
            else
            {
                decoded_val = 0;
                scratch_bintree_raw[table_idx] = 0;
                bit_shift_state_4 = bit_shift_state_6;
                bits_to_read = ((bit_shift_state_1 & 1) * 2 + (bit_shift_state_2 & 1)) * 2 + (bit_shift_state_3 & 1) + 1;
                do
                {
                    bit_shift_state_6 = bit_shift_state_4 >> 1;
                    temp_bit_value = bit_shift_state_4;
                    if (bit_shift_state_4 >> 1 == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = current_byte_value >> 1 | 0x80;
                        temp_bit_value = (uint32_t)current_byte_value;
                    }
                    bit_shift_state_4 = bit_shift_state_6;
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (temp_bit_value & 1);
                } while (bits_to_read != 0);
                scratch_bintree_raw[table_idx + 0x200] = (uint8_t)decoded_val;
            }
            block_size = block_size + -1;
            table_idx = table_idx + 1;
        } while (block_size != 0);
        /* build bintree_pos */
        if (in_ptr < in_end)
        {
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        else
        {
            ReadBlock(file, buffer, in_ptr, in_end);
            current_byte_value = *in_ptr;
            temp_read_ptr = in_ptr;
        }
        in_ptr = temp_read_ptr + 1;
        bit_shift_state_7 = bit_shift_state_4 >> 1;
        block_size = (uint32_t)current_byte_value << 1;
        if (bit_shift_state_7 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
                in_ptr = temp_read_ptr + 2;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
                in_ptr = in_ptr + 1;
            }
            bit_shift_state_4 = (uint32_t)current_byte_value;
            bit_shift_state_7 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_1 = bit_shift_state_7 >> 1;
        if (bit_shift_state_1 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_7 = (uint32_t)current_byte_value;
            bit_shift_state_1 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_2 = bit_shift_state_1 >> 1;
        if (bit_shift_state_2 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_1 = (uint32_t)current_byte_value;
            bit_shift_state_2 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_3 = bit_shift_state_2 >> 1;
        if (bit_shift_state_3 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_2 = (uint32_t)current_byte_value;
            bit_shift_state_3 = current_byte_value >> 1 | 0x80;
        }
        bit_shift_state_5 = bit_shift_state_3 >> 1;
        if (bit_shift_state_5 == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_3 = (uint32_t)current_byte_value;
            bit_shift_state_5 = current_byte_value >> 1 | 0x80;
        }
        bit_cache = bit_shift_state_5 >> 1;
        if (bit_cache == 0)
        {
            if (in_ptr < in_end)
            {
                current_byte_value = *in_ptr;
            }
            else
            {
                ReadBlock(file, buffer, in_ptr, in_end);
                current_byte_value = *in_ptr;
            }
            in_ptr = in_ptr + 1;
            bit_shift_state_5 = (uint32_t)current_byte_value;
            bit_cache = current_byte_value >> 1 | 0x80;
        }
        table_idx = 0;
        do
        {
            bit_shift_state_6 = bit_cache >> 1;
            if (bit_shift_state_6 == 0)
            {
                if (in_ptr < in_end)
                {
                    current_byte_value = *in_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, in_ptr, in_end);
                    current_byte_value = *in_ptr;
                }
                in_ptr = in_ptr + 1;
                bit_cache = (uint32_t)current_byte_value;
                bit_shift_state_6 = current_byte_value >> 1 | 0x80;
            }
            decoded_val = 0;
            bits_to_read = ((bit_shift_state_4 & 1) * 2 + (bit_shift_state_7 & 1)) * 2 + (bit_shift_state_1 & 1) + 1;
            if ((bit_cache & 1) == 0)
            {
                do
                {
                    bit_cache = bit_shift_state_6 >> 1;
                    if (bit_cache == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = (uint32_t)current_byte_value;
                        bit_cache = current_byte_value >> 1 | 0x80;
                    }
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (bit_shift_state_6 & 1);
                    bit_shift_state_6 = bit_cache;
                } while (bits_to_read != 0);
                scratch_bintree_pos[table_idx] = (uint8_t)decoded_val;
                scratch_bintree_pos[table_idx + 0x200] = 0;
            }
            else
            {
                decoded_val = 0;
                scratch_bintree_pos[table_idx] = 0;
                bit_cache = bit_shift_state_6;
                bits_to_read = ((bit_shift_state_2 & 1) * 2 + (bit_shift_state_3 & 1)) * 2 + (bit_shift_state_5 & 1) + 1;
                do
                {
                    bit_shift_state_6 = bit_cache >> 1;
                    temp_bit_value = bit_cache;
                    if (bit_cache >> 1 == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_6 = current_byte_value >> 1 | 0x80;
                        temp_bit_value = (uint32_t)current_byte_value;
                    }
                    bit_cache = bit_shift_state_6;
                    bits_to_read = bits_to_read + -1;
                    decoded_val = decoded_val * 2 + (temp_bit_value & 1);
                } while (bits_to_read != 0);
                scratch_bintree_pos[table_idx + 0x200] = (uint8_t)decoded_val;
            }
            block_size = block_size + -1;
            table_idx = table_idx + 1;
        } while (block_size != 0);
        do
        {
            /* decode */
            // prefetch(in_ptr + 0x40,0);
            bit_shift_state_7 = 0;
            do
            {
                bit_shift_state_1 = bit_cache >> 1;
                bit_shift_state_2 = bit_cache;
                if (bit_cache >> 1 == 0)
                {
                    if (in_ptr < in_end)
                    {
                        current_byte_value = *in_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, in_ptr, in_end);
                        current_byte_value = *in_ptr;
                    }
                    in_ptr = in_ptr + 1;
                    bit_shift_state_1 = current_byte_value >> 1 | 0x80;
                    bit_shift_state_2 = (uint32_t)current_byte_value;
                }
                bit_cache = bit_shift_state_1;
                block_size = bit_shift_state_7 * 2 + (bit_shift_state_2 & 1);
                bit_shift_state_7 = (uint32_t)scratch_bintree_raw[block_size];
            } while (bit_shift_state_7 != 0);
            bit_shift_state_7 = (uint32_t)scratch_bintree_raw[block_size + 0x200];
            bit_shift_state_1 = 0;
            if (bit_shift_state_7 != 0)
            {
                do
                {
                    if (in_ptr < in_end)
                    {
                        decoded_output_byte = *in_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, in_ptr, in_end);
                        decoded_output_byte = *in_ptr;
                    }
                    in_ptr = in_ptr + 1;
                    buffer[buffer_index] = decoded_output_byte;
                    bit_shift_state_7 = bit_shift_state_7 - 1;
                    buffer_index++;
                } while (bit_shift_state_7 != 0);
                bit_shift_state_1 = 0;
            }
            do
            {
                bit_shift_state_7 = bit_cache >> 1;
                bit_shift_state_2 = bit_cache;
                if (bit_cache >> 1 == 0)
                {
                    if (in_ptr < in_end)
                    {
                        current_byte_value = *in_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, in_ptr, in_end);
                        current_byte_value = *in_ptr;
                    }
                    in_ptr = in_ptr + 1;
                    bit_shift_state_7 = current_byte_value >> 1 | 0x80;
                    bit_shift_state_2 = (uint32_t)current_byte_value;
                }
                bit_cache = bit_shift_state_7;
                block_size = bit_shift_state_1 * 2 + (bit_shift_state_2 & 1);
                bit_shift_state_1 = (uint32_t)scratch_bintree_size[block_size];
            } while (bit_shift_state_1 != 0);
            bit_shift_state_7 = (uint32_t)scratch_bintree_size[block_size + 0x200];
            if (bit_shift_state_7 == 0xff)
            {
                bit_shift_state_7 = bit_cache >> 1;
                if (bit_shift_state_7 == 0)
                {
                    if (in_ptr < in_end)
                    {
                        current_byte_value = *in_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, in_ptr, in_end);
                        current_byte_value = *in_ptr;
                    }
                    in_ptr = in_ptr + 1;
                    bit_cache = (uint32_t)current_byte_value;
                    bit_shift_state_7 = current_byte_value >> 1 | 0x80;
                }
                bit_shift_state_1 = bit_cache & 1;
                bit_cache = bit_shift_state_7 >> 1;
                if (bit_cache == 0)
                {
                    if (in_ptr < in_end)
                    {
                        current_byte_value = *in_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, in_ptr, in_end);
                        current_byte_value = *in_ptr;
                    }
                    in_ptr = in_ptr + 1;
                    bit_shift_state_7 = (uint32_t)current_byte_value;
                    bit_cache = current_byte_value >> 1 | 0x80;
                }
                if (in_ptr < in_end)
                {
                    current_byte_value = *in_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, in_ptr, in_end);
                    current_byte_value = *in_ptr;
                }
                in_ptr = in_ptr + 1;
                bit_shift_state_7 = ((bit_shift_state_1 << 1 | (bit_shift_state_7 & 1)) << 8 | (uint32_t)current_byte_value) + 0xff;
            }
            if (bit_shift_state_7 != 0)
            {
                bit_shift_state_2 = 0;
                bit_shift_state_1 = bit_cache;
                do
                {
                    bit_cache = bit_shift_state_1 >> 1;
                    if (bit_cache == 0)
                    {
                        if (in_ptr < in_end)
                        {
                            current_byte_value = *in_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, in_ptr, in_end);
                            current_byte_value = *in_ptr;
                        }
                        in_ptr = in_ptr + 1;
                        bit_shift_state_1 = (uint32_t)current_byte_value;
                        bit_cache = current_byte_value >> 1 | 0x80;
                    }
                    block_size = bit_shift_state_2 * 2 + (bit_shift_state_1 & 1);
                    bit_shift_state_2 = (uint32_t)scratch_bintree_pos[block_size];
                    bit_shift_state_1 = bit_cache;
                } while (bit_shift_state_2 != 0);
                current_byte_value = scratch_bintree_pos[block_size + 0x200];
                block_size = current_byte_value - 1;
                if (current_byte_value < 2)
                {
                    bit_shift_state_2 = ~(uint32_t)current_byte_value;
                }
                else
                {
                    bit_shift_state_2 = 1;
                    do
                    {
                        bit_cache = bit_shift_state_1 >> 1;
                        if (bit_cache == 0)
                        {
                            if (in_ptr < in_end)
                            {
                                current_byte_value = *in_ptr;
                            }
                            else
                            {
                                ReadBlock(file, buffer, in_ptr, in_end);
                                current_byte_value = *in_ptr;
                            }
                            in_ptr = in_ptr + 1;
                            bit_shift_state_1 = (uint32_t)current_byte_value;
                            bit_cache = current_byte_value >> 1 | 0x80;
                        }
                        block_size = block_size + -1;
                        bit_shift_state_2 = bit_shift_state_2 * 2 + (bit_shift_state_1 & 1);
                        bit_shift_state_1 = bit_cache;
                    } while (block_size != 0);
                    bit_shift_state_2 = ~bit_shift_state_2;
                }
                do
                {
                    bit_shift_state_7 = bit_shift_state_7 - 1;
                    int32_t rel = static_cast<int32_t>(bit_shift_state_2);
                    size_t src_index = static_cast<size_t>(static_cast<int64_t>(buffer_index) + static_cast<int64_t>(rel));
                    buffer[buffer_index] = buffer[src_index];
                    buffer_index++;
                } while (bit_shift_state_7 != 0);
            }
            num_blocks = num_blocks - 1;
        } while (num_blocks != 0);
    }
    return;
}

DECOMP DecompressTo(std::ifstream &file, std::vector<char> &buffer, uint32_t packed_size)
{
    (void)packed_size;
    evo_block_header_t block_header;
    if (Open(file, block_header) != DECOMP::CONTINUE)
        return DECOMP::END;

    if (block_header.block_tag == BLOCK_TAG_RAW0)
    {
        if (block_header.unpacked_size > buffer.size())
        {
            std::cerr << "RAW block unpacked size exceeds destination buffer." << std::endl;
            return DECOMP::ERROR;
        }

        if (!file.read(buffer.data(), block_header.unpacked_size))
        {
            std::cerr << "Failed to read RAW block payload." << std::endl;
            return DECOMP::ERROR;
        }
        g_block_remaining = 0;
        return DECOMP::CONTINUE;
    }

    Unpack(buffer, file);

    return DECOMP::CONTINUE;
}

static void reset_evo_decoder_state()
{
    memset(scratch_bintree_size, 0, sizeof(scratch_bintree_size));
    memset(scratch_bintree_pos, 0, sizeof(scratch_bintree_pos));
    memset(scratch_bintree_raw, 0, sizeof(scratch_bintree_raw));
    g_block_remaining = 0;
}

void rebuild_datafile(const char *decompressed_file, const char *output_dat)
{
    std::ifstream in(decompressed_file, std::ios::binary);
    if (!in)
    {
        std::cerr << "Failed to open decompressed input: " << decompressed_file << std::endl;
        return;
    }

    std::vector<char> input_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (input_data.empty())
    {
        std::cerr << "Input is empty: " << decompressed_file << std::endl;
        return;
    }

    evo_dat_header_t dat_header{};
    memcpy(&dat_header.magic, EVOB_MAGIC, 4);
    dat_header.unpacked_size = static_cast<uint32_t>(input_data.size());
    dat_header.packed_size = 0x80;

    evo_block_header_t block_header{};
    memcpy(&block_header.magic, BLOCK_MAGIC, 4);
    block_header.unpacked_size = dat_header.unpacked_size;
    block_header.packed_size = dat_header.unpacked_size;
    block_header.block_tag = BLOCK_TAG_RAW0;

    std::ofstream out(output_dat, std::ios::binary);
    if (!out)
    {
        std::cerr << "Failed to open output EVO: " << output_dat << std::endl;
        return;
    }

    out.write(reinterpret_cast<const char *>(&dat_header), sizeof(dat_header));
    out.write(reinterpret_cast<const char *>(&block_header), sizeof(block_header));
    out.write(input_data.data(), static_cast<std::streamsize>(input_data.size()));
    out.close();

    if (DEBUG)
    {
        std::cout << "Recompressed EVO written (RAW0 generic): " << output_dat << std::endl;
    }
}

void unpack_datafile(const char *datafile, const char *output_dir)
{
    DECOMP status = DECOMP::CONTINUE;
    uint32_t block_index = 0;

    // init bintree
    reset_evo_decoder_state();

    std::ifstream file(datafile, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open " << datafile << std::endl;
        return;
    }
    evo_dat_header_t header;
    file.read(reinterpret_cast<char *>(&header), sizeof(evo_dat_header_t));
    if (memcmp(reinterpret_cast<const void *>(&header.magic), EVOA_MAGIC, 4) != 0 &&
        memcmp(reinterpret_cast<const void *>(&header.magic), EVO9_MAGIC, 4) != 0 &&
        memcmp(reinterpret_cast<const void *>(&header.magic), EVOB_MAGIC, 4) != 0)
    {
        std::cerr << datafile << " is not a valid EVO data file." << std::endl;
        return;
    }
    if (header.packed_size == 0 || header.unpacked_size == 0)
    {
        std::cerr << "Bad values in header of " << datafile << std::endl;
        return;
    }
    if (header.packed_size < 0x10)
        header.packed_size = 0x10; // force minimum size?

    if (DEBUG)
    {
        std::cout << "Magic: " << std::string(reinterpret_cast<char *>(&header.magic), 4) << std::endl;
        std::cout << "Unpacked size: " << header.unpacked_size << std::endl;
        std::cout << "Packed size: " << header.packed_size << std::endl
                  << std::endl;
    }

    std::filesystem::create_directories(output_dir);

    std::vector<char> buffer(header.unpacked_size);
    status = DecompressTo(file, buffer, header.unpacked_size);
    if (status == DECOMP::END)
    {
        std::cerr << "Decompression ended unexpectedly." << std::endl;
        return;
    }
    std::string input_name = datafile;
    size_t sep = input_name.find_last_of("/\\");
    if (sep != std::string::npos)
        input_name = input_name.substr(sep + 1);
    std::filesystem::path out_path = std::filesystem::path(output_dir) / (input_name + ".decompressed");
    std::ofstream out_file(out_path, std::ios::binary);
    if (!out_file) {
        std::cerr << "Failed to open output file: " << out_path << std::endl;
        return;
    }
    out_file.write(buffer.data(), buffer.size());
    out_file.close();
    if (DEBUG) {
        std::cout << "Decompressed data written to: " << out_path << std::endl;
    }
    while (status == DECOMP::CONTINUE)
    {
        status = DecompressTo(file, buffer, header.unpacked_size);
        if (status != DECOMP::CONTINUE)
            break;

        std::string block_path = out_path.string() + ".block" + std::to_string(block_index++);
        std::ofstream out_file_next(block_path, std::ios::binary);
        if (!out_file_next) {
            std::cerr << "Failed to open output file for block: " << block_path << std::endl;
        } else {
            out_file_next.write(buffer.data(), buffer.size());
            out_file_next.close();
            if (DEBUG) {
                std::cout << "Decompressed block written to: " << block_path << std::endl;
            }
        }
    }
    return;
}