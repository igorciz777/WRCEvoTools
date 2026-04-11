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
        if (DEBUG)
            std::cerr << "Failed to read block header. EOF or error" << std::endl;
        return DECOMP::END;
    }
    if (memcmp(reinterpret_cast<const void *>(&block_header.magic), BLOCK_MAGIC, 4) != 0)
    {
        std::cerr << "Invalid block magic: " << std::hex << block_header.magic << std::dec << std::endl;
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
    unsigned char bVar1;
    int iVar1;
    unsigned char ucVar4;
    uint32_t uVar4;
    int iVar2;
    int iVar3;
    int iVar4;
    uint32_t uVar8;
    uint32_t uVar9;
    uint32_t uVar10;
    uint32_t uVar11;
    uint32_t uVar12;
    uint32_t uVar13;
    uint32_t uVar14;
    uint32_t uVar15;
    uint32_t uVar16;
    unsigned char *out_ptr;
    unsigned char *fs_ptr;
    unsigned char ucVar1;
    unsigned char ucVar2;
    unsigned char ucVar3;
    unsigned char *pVar1;
    size_t buffer_index = 0;

    uVar8 = 1;
    fs_ptr = 0x0;
    out_ptr = 0x0;
    while (true)
    {
        /* build bintree_size */
        uVar15 = uVar8 >> 1;
        if (uVar15 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                ucVar2 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                ucVar2 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar8 = (uint32_t)ucVar2;
            uVar15 = ucVar2 >> 1 | 0x80;
        }
        if ((uVar8 & 1) == 0)
            break;
        if (fs_ptr < out_ptr)
        {
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        else
        {
            ReadBlock(file, buffer, fs_ptr, out_ptr);
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        fs_ptr = pVar1 + 1;
        if (fs_ptr < out_ptr)
        {
            ucVar3 = *fs_ptr;
            fs_ptr = pVar1 + 2;
        }
        else
        {
            ReadBlock(file, buffer, fs_ptr, out_ptr);
            ucVar3 = *fs_ptr;
            fs_ptr = fs_ptr + 1;
        }
        uVar16 = (uint32_t)bVar1 + (uint32_t)ucVar3 * 0x100;
        if (fs_ptr < out_ptr)
        {
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        else
        {
            ReadBlock(file, buffer, fs_ptr, out_ptr);
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        fs_ptr = pVar1 + 1;
        uVar8 = uVar15 >> 1;
        iVar4 = (uint32_t)bVar1 << 1;
        if (uVar8 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
                fs_ptr = pVar1 + 2;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
                fs_ptr = fs_ptr + 1;
            }
            uVar15 = (uint32_t)bVar1;
            uVar8 = bVar1 >> 1 | 0x80;
        }
        uVar9 = uVar8 >> 1;
        if (uVar9 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar8 = (uint32_t)bVar1;
            uVar9 = bVar1 >> 1 | 0x80;
        }
        uVar10 = uVar9 >> 1;
        if (uVar10 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar9 = (uint32_t)bVar1;
            uVar10 = bVar1 >> 1 | 0x80;
        }
        uVar11 = uVar10 >> 1;
        if (uVar11 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar10 = (uint32_t)bVar1;
            uVar11 = bVar1 >> 1 | 0x80;
        }
        uVar12 = uVar11 >> 1;
        if (uVar12 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar11 = (uint32_t)bVar1;
            uVar12 = bVar1 >> 1 | 0x80;
        }
        uVar13 = uVar12 >> 1;
        if (uVar13 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar12 = (uint32_t)bVar1;
            uVar13 = bVar1 >> 1 | 0x80;
        }
        iVar3 = 0;
        do
        {
            uVar14 = uVar13 >> 1;
            if (uVar14 == 0)
            {
                if (fs_ptr < out_ptr)
                {
                    bVar1 = *fs_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, fs_ptr, out_ptr);
                    bVar1 = *fs_ptr;
                }
                fs_ptr = fs_ptr + 1;
                uVar13 = (uint32_t)bVar1;
                uVar14 = bVar1 >> 1 | 0x80;
            }
            iVar1 = 0;
            iVar2 = ((uVar15 & 1) * 2 + (uVar8 & 1)) * 2 + (uVar9 & 1) + 1;
            if ((uVar13 & 1) == 0)
            {
                do
                {
                    uVar13 = uVar14 >> 1;
                    if (uVar13 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = (uint32_t)bVar1;
                        uVar13 = bVar1 >> 1 | 0x80;
                    }
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar14 & 1);
                    uVar14 = uVar13;
                } while (iVar2 != 0);
                scratch_bintree_size[iVar3] = (uint8_t)iVar1;
                scratch_bintree_size[iVar3 + 0x200] = 0;
            }
            else
            {
                iVar1 = 0;
                scratch_bintree_size[iVar3] = 0;
                uVar13 = uVar14;
                iVar2 = ((uVar10 & 1) * 2 + (uVar11 & 1)) * 2 + (uVar12 & 1) + 1;
                do
                {
                    uVar14 = uVar13 >> 1;
                    uVar4 = uVar13;
                    if (uVar13 >> 1 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = bVar1 >> 1 | 0x80;
                        uVar4 = (uint32_t)bVar1;
                    }
                    uVar13 = uVar14;
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar4 & 1);
                } while (iVar2 != 0);
                scratch_bintree_size[iVar3 + 0x200] = (uint8_t)iVar1;
            }
            iVar4 = iVar4 + -1;
            iVar3 = iVar3 + 1;
        } while (iVar4 != 0);
        /* build bintree_raw */
        if (fs_ptr < out_ptr)
        {
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        else
        {
            ReadBlock(file, buffer, fs_ptr, out_ptr);
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        fs_ptr = pVar1 + 1;
        uVar8 = uVar13 >> 1;
        iVar4 = (uint32_t)bVar1 << 1;
        if (uVar8 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
                fs_ptr = pVar1 + 2;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
                fs_ptr = fs_ptr + 1;
            }
            uVar13 = (uint32_t)bVar1;
            uVar8 = bVar1 >> 1 | 0x80;
        }
        uVar15 = uVar8 >> 1;
        if (uVar15 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar8 = (uint32_t)bVar1;
            uVar15 = bVar1 >> 1 | 0x80;
        }
        uVar9 = uVar15 >> 1;
        if (uVar9 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar15 = (uint32_t)bVar1;
            uVar9 = bVar1 >> 1 | 0x80;
        }
        uVar10 = uVar9 >> 1;
        if (uVar10 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar9 = (uint32_t)bVar1;
            uVar10 = bVar1 >> 1 | 0x80;
        }
        uVar11 = uVar10 >> 1;
        if (uVar11 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar10 = (uint32_t)bVar1;
            uVar11 = bVar1 >> 1 | 0x80;
        }
        uVar12 = uVar11 >> 1;
        if (uVar12 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar11 = (uint32_t)bVar1;
            uVar12 = bVar1 >> 1 | 0x80;
        }
        iVar3 = 0;
        do
        {
            uVar14 = uVar12 >> 1;
            if (uVar14 == 0)
            {
                if (fs_ptr < out_ptr)
                {
                    bVar1 = *fs_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, fs_ptr, out_ptr);
                    bVar1 = *fs_ptr;
                }
                fs_ptr = fs_ptr + 1;
                uVar12 = (uint32_t)bVar1;
                uVar14 = bVar1 >> 1 | 0x80;
            }
            iVar1 = 0;
            iVar2 = ((uVar13 & 1) * 2 + (uVar8 & 1)) * 2 + (uVar15 & 1) + 1;
            if ((uVar12 & 1) == 0)
            {
                do
                {
                    uVar12 = uVar14 >> 1;
                    if (uVar12 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = (uint32_t)bVar1;
                        uVar12 = bVar1 >> 1 | 0x80;
                    }
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar14 & 1);
                    uVar14 = uVar12;
                } while (iVar2 != 0);
                scratch_bintree_raw[iVar3] = (uint8_t)iVar1;
                scratch_bintree_raw[iVar3 + 0x200] = 0;
            }
            else
            {
                iVar1 = 0;
                scratch_bintree_raw[iVar3] = 0;
                uVar12 = uVar14;
                iVar2 = ((uVar9 & 1) * 2 + (uVar10 & 1)) * 2 + (uVar11 & 1) + 1;
                do
                {
                    uVar14 = uVar12 >> 1;
                    uVar4 = uVar12;
                    if (uVar12 >> 1 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = bVar1 >> 1 | 0x80;
                        uVar4 = (uint32_t)bVar1;
                    }
                    uVar12 = uVar14;
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar4 & 1);
                } while (iVar2 != 0);
                scratch_bintree_raw[iVar3 + 0x200] = (uint8_t)iVar1;
            }
            iVar4 = iVar4 + -1;
            iVar3 = iVar3 + 1;
        } while (iVar4 != 0);
        /* build bintree_pos */
        if (fs_ptr < out_ptr)
        {
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        else
        {
            ReadBlock(file, buffer, fs_ptr, out_ptr);
            bVar1 = *fs_ptr;
            pVar1 = fs_ptr;
        }
        fs_ptr = pVar1 + 1;
        uVar15 = uVar12 >> 1;
        iVar4 = (uint32_t)bVar1 << 1;
        if (uVar15 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
                fs_ptr = pVar1 + 2;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
                fs_ptr = fs_ptr + 1;
            }
            uVar12 = (uint32_t)bVar1;
            uVar15 = bVar1 >> 1 | 0x80;
        }
        uVar9 = uVar15 >> 1;
        if (uVar9 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar15 = (uint32_t)bVar1;
            uVar9 = bVar1 >> 1 | 0x80;
        }
        uVar10 = uVar9 >> 1;
        if (uVar10 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar9 = (uint32_t)bVar1;
            uVar10 = bVar1 >> 1 | 0x80;
        }
        uVar11 = uVar10 >> 1;
        if (uVar11 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar10 = (uint32_t)bVar1;
            uVar11 = bVar1 >> 1 | 0x80;
        }
        uVar13 = uVar11 >> 1;
        if (uVar13 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar11 = (uint32_t)bVar1;
            uVar13 = bVar1 >> 1 | 0x80;
        }
        uVar8 = uVar13 >> 1;
        if (uVar8 == 0)
        {
            if (fs_ptr < out_ptr)
            {
                bVar1 = *fs_ptr;
            }
            else
            {
                ReadBlock(file, buffer, fs_ptr, out_ptr);
                bVar1 = *fs_ptr;
            }
            fs_ptr = fs_ptr + 1;
            uVar13 = (uint32_t)bVar1;
            uVar8 = bVar1 >> 1 | 0x80;
        }
        iVar3 = 0;
        do
        {
            uVar14 = uVar8 >> 1;
            if (uVar14 == 0)
            {
                if (fs_ptr < out_ptr)
                {
                    bVar1 = *fs_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, fs_ptr, out_ptr);
                    bVar1 = *fs_ptr;
                }
                fs_ptr = fs_ptr + 1;
                uVar8 = (uint32_t)bVar1;
                uVar14 = bVar1 >> 1 | 0x80;
            }
            iVar1 = 0;
            iVar2 = ((uVar12 & 1) * 2 + (uVar15 & 1)) * 2 + (uVar9 & 1) + 1;
            if ((uVar8 & 1) == 0)
            {
                do
                {
                    uVar8 = uVar14 >> 1;
                    if (uVar8 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = (uint32_t)bVar1;
                        uVar8 = bVar1 >> 1 | 0x80;
                    }
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar14 & 1);
                    uVar14 = uVar8;
                } while (iVar2 != 0);
                scratch_bintree_pos[iVar3] = (uint8_t)iVar1;
                scratch_bintree_pos[iVar3 + 0x200] = 0;
            }
            else
            {
                iVar1 = 0;
                scratch_bintree_pos[iVar3] = 0;
                uVar8 = uVar14;
                iVar2 = ((uVar10 & 1) * 2 + (uVar11 & 1)) * 2 + (uVar13 & 1) + 1;
                do
                {
                    uVar14 = uVar8 >> 1;
                    uVar4 = uVar8;
                    if (uVar8 >> 1 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar14 = bVar1 >> 1 | 0x80;
                        uVar4 = (uint32_t)bVar1;
                    }
                    uVar8 = uVar14;
                    iVar2 = iVar2 + -1;
                    iVar1 = iVar1 * 2 + (uVar4 & 1);
                } while (iVar2 != 0);
                scratch_bintree_pos[iVar3 + 0x200] = (uint8_t)iVar1;
            }
            iVar4 = iVar4 + -1;
            iVar3 = iVar3 + 1;
        } while (iVar4 != 0);
        do
        {
            /* decode */
            // prefetch(fs_ptr + 0x40,0);
            uVar15 = 0;
            do
            {
                uVar9 = uVar8 >> 1;
                uVar10 = uVar8;
                if (uVar8 >> 1 == 0)
                {
                    if (fs_ptr < out_ptr)
                    {
                        bVar1 = *fs_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, fs_ptr, out_ptr);
                        bVar1 = *fs_ptr;
                    }
                    fs_ptr = fs_ptr + 1;
                    uVar9 = bVar1 >> 1 | 0x80;
                    uVar10 = (uint32_t)bVar1;
                }
                uVar8 = uVar9;
                iVar4 = uVar15 * 2 + (uVar10 & 1);
                uVar15 = (uint32_t)scratch_bintree_raw[iVar4];
            } while (uVar15 != 0);
            uVar15 = (uint32_t)scratch_bintree_raw[iVar4 + 0x200];
            uVar9 = 0;
            if (uVar15 != 0)
            {
                do
                {
                    if (fs_ptr < out_ptr)
                    {
                        ucVar4 = *fs_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, fs_ptr, out_ptr);
                        ucVar4 = *fs_ptr;
                    }
                    fs_ptr = fs_ptr + 1;
                    buffer[buffer_index] = ucVar4;
                    uVar15 = uVar15 - 1;
                    buffer_index++;
                } while (uVar15 != 0);
                uVar9 = 0;
            }
            do
            {
                uVar15 = uVar8 >> 1;
                uVar10 = uVar8;
                if (uVar8 >> 1 == 0)
                {
                    if (fs_ptr < out_ptr)
                    {
                        bVar1 = *fs_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, fs_ptr, out_ptr);
                        bVar1 = *fs_ptr;
                    }
                    fs_ptr = fs_ptr + 1;
                    uVar15 = bVar1 >> 1 | 0x80;
                    uVar10 = (uint32_t)bVar1;
                }
                uVar8 = uVar15;
                iVar4 = uVar9 * 2 + (uVar10 & 1);
                uVar9 = (uint32_t)scratch_bintree_size[iVar4];
            } while (uVar9 != 0);
            uVar15 = (uint32_t)scratch_bintree_size[iVar4 + 0x200];
            if (uVar15 == 0xff)
            {
                uVar15 = uVar8 >> 1;
                if (uVar15 == 0)
                {
                    if (fs_ptr < out_ptr)
                    {
                        bVar1 = *fs_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, fs_ptr, out_ptr);
                        bVar1 = *fs_ptr;
                    }
                    fs_ptr = fs_ptr + 1;
                    uVar8 = (uint32_t)bVar1;
                    uVar15 = bVar1 >> 1 | 0x80;
                }
                uVar9 = uVar8 & 1;
                uVar8 = uVar15 >> 1;
                if (uVar8 == 0)
                {
                    if (fs_ptr < out_ptr)
                    {
                        bVar1 = *fs_ptr;
                    }
                    else
                    {
                        ReadBlock(file, buffer, fs_ptr, out_ptr);
                        bVar1 = *fs_ptr;
                    }
                    fs_ptr = fs_ptr + 1;
                    uVar15 = (uint32_t)bVar1;
                    uVar8 = bVar1 >> 1 | 0x80;
                }
                if (fs_ptr < out_ptr)
                {
                    bVar1 = *fs_ptr;
                }
                else
                {
                    ReadBlock(file, buffer, fs_ptr, out_ptr);
                    bVar1 = *fs_ptr;
                }
                fs_ptr = fs_ptr + 1;
                uVar15 = ((uVar9 << 1 | uVar15 & 1) << 8 | (uint32_t)bVar1) + 0xff;
            }
            if (uVar15 != 0)
            {
                uVar10 = 0;
                uVar9 = uVar8;
                do
                {
                    uVar8 = uVar9 >> 1;
                    if (uVar8 == 0)
                    {
                        if (fs_ptr < out_ptr)
                        {
                            bVar1 = *fs_ptr;
                        }
                        else
                        {
                            ReadBlock(file, buffer, fs_ptr, out_ptr);
                            bVar1 = *fs_ptr;
                        }
                        fs_ptr = fs_ptr + 1;
                        uVar9 = (uint32_t)bVar1;
                        uVar8 = bVar1 >> 1 | 0x80;
                    }
                    iVar4 = uVar10 * 2 + (uVar9 & 1);
                    uVar10 = (uint32_t)scratch_bintree_pos[iVar4];
                    uVar9 = uVar8;
                } while (uVar10 != 0);
                bVar1 = scratch_bintree_pos[iVar4 + 0x200];
                iVar4 = bVar1 - 1;
                if (bVar1 < 2)
                {
                    uVar10 = ~(uint32_t)bVar1;
                }
                else
                {
                    uVar10 = 1;
                    do
                    {
                        uVar8 = uVar9 >> 1;
                        if (uVar8 == 0)
                        {
                            if (fs_ptr < out_ptr)
                            {
                                bVar1 = *fs_ptr;
                            }
                            else
                            {
                                ReadBlock(file, buffer, fs_ptr, out_ptr);
                                bVar1 = *fs_ptr;
                            }
                            fs_ptr = fs_ptr + 1;
                            uVar9 = (uint32_t)bVar1;
                            uVar8 = bVar1 >> 1 | 0x80;
                        }
                        iVar4 = iVar4 + -1;
                        uVar10 = uVar10 * 2 + (uVar9 & 1);
                        uVar9 = uVar8;
                    } while (iVar4 != 0);
                    uVar10 = ~uVar10;
                }
                do
                {
                    uVar15 = uVar15 - 1;
                    int32_t rel = static_cast<int32_t>(uVar10);
                    size_t src_index = static_cast<size_t>(static_cast<int64_t>(buffer_index) + static_cast<int64_t>(rel));
                    buffer[buffer_index] = buffer[src_index];
                    buffer_index++;
                } while (uVar15 != 0);
            }
            uVar16 = uVar16 - 1;
        } while (uVar16 != 0);
    }
    return;
}

DECOMP DecompressTo(std::ifstream &file, std::vector<char> &buffer, uint32_t packed_size)
{
    evo_block_header_t block_header;
    if (Open(file, block_header) != DECOMP::CONTINUE)
        return DECOMP::END;

    Unpack(buffer, file);

    return DECOMP::CONTINUE;
}

void unpack_datafile(const char *datafile, const char *output_dir)
{
    DECOMP status = DECOMP::CONTINUE;
    uint32_t block_index = 0;

    // init bintree
    memset(scratch_bintree_size, 0, sizeof(scratch_bintree_size));
    memset(scratch_bintree_pos, 0, sizeof(scratch_bintree_pos));
    memset(scratch_bintree_raw, 0, sizeof(scratch_bintree_raw));

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