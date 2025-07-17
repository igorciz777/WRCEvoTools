#include <iostream>
#include <filesystem>
#include <cstdint>
#include <fstream>

#include "hash.hpp"
#include "filenames.hpp"

typedef struct
{
  uint32_t file_count;
  uint32_t hash_a;
  uint32_t hash_b;
} dvd_bin_header_t;

typedef struct
{
  uint32_t offset;
  uint32_t size;
  uint32_t hash;
} dvd_bin_entry_t;

void unpack_dvd(const char *dvd_bin, const char *dvd_dat, const char *output_dir)
{
    std::ifstream file_bin(dvd_bin, std::ios::binary);
    if (!file_bin)
    {
        std::cerr << "Failed to open " << dvd_bin << std::endl;
        return;
    }

    std::ifstream file_dat(dvd_dat, std::ios::binary);
    if (!file_dat)
    {
        std::cerr << "Failed to open " << dvd_dat << std::endl;
        return;
    }

    std::filesystem::create_directory(output_dir);

    dvd_bin_header_t header;
    file_bin.read(reinterpret_cast<char *>(&header), sizeof(dvd_bin_header_t));

    std::cout << "File count: " << header.file_count << std::endl;
    std::cout << "Hash A: " << std::hex << header.hash_a << std::endl;
    std::cout << "Hash B: " << std::hex << header.hash_b << std::endl;

    auto entries = new dvd_bin_entry_t[header.file_count];
    file_bin.read(reinterpret_cast<char *>(entries), sizeof(dvd_bin_entry_t) * header.file_count);

    for (uint32_t i = 0; i < header.file_count; i++)
    {
        std::cout << "Entry " << i << std::endl;
        std::cout << "  Offset: " << entries[i].offset << std::endl;
        std::cout << "  Size: " << entries[i].size << std::endl;
        std::cout << "  Hash: " << std::hex << entries[i].hash << std::endl;

        file_dat.seekg(entries[i].offset, std::ios_base::beg);

        std::vector<char> data(entries[i].size);
        file_dat.read(data.data(), entries[i].size);

        std::stringstream ss, to_hex;
        to_hex << std::hex << std::setfill('0') << std::setw(8) << entries[i].hash << ".dat";
        std::string filename = to_hex.str();
        ss << output_dir << "/";

        for (const std::string &name : filenames)
        {
            const char *c_name = name.c_str();
            if (entries[i].hash == str_to_hash(c_name, 0x7AB7, 0x6A2F))
            {
                filename = name;
                break;
            }
        }
        ss << filename;
        std::ofstream file_out(ss.str(), std::ios::binary);
        if (!file_out)
        {
            std::cerr << "Failed to open " << ss.str() << std::endl;
            continue;
        }
        file_out.write(data.data(), entries[i].size);
        file_out.close();
    }
    delete[] entries;
    file_bin.close();
    file_dat.close();
    std::cout << "Unpacking completed." << std::endl;
}