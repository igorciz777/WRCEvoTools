#include <iostream>
#include <filesystem>
#include <cstdint>
#include <fstream>

#include "hash.hpp"
#include <algorithm>

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
        to_hex << std::hex << std::setfill('0') << std::setw(8) << entries[i].hash << ".hashed";
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

void rebuild_dvd(const char *dvd_bin_out, const char *dvd_dat_out, const char *input_dir)
{
    std::ofstream file_bin(dvd_bin_out, std::ios::binary);
    if (!file_bin)
    {
        std::cerr << "Failed to open " << dvd_bin_out << std::endl;
        return;
    }

    std::ofstream file_dat(dvd_dat_out, std::ios::binary);
    if (!file_dat)
    {
        std::cerr << "Failed to open " << dvd_dat_out << std::endl;
        return;
    }

    dvd_bin_header_t header = {0, 0x7AB7, 0x6A2F};
    std::vector<dvd_bin_entry_t> entries;

    for (const auto &entry : std::filesystem::directory_iterator(input_dir))
    {
        if (entry.is_regular_file())
        {
            dvd_bin_entry_t bin_entry;
            bin_entry.offset = static_cast<uint32_t>(file_dat.tellp());
            bin_entry.size = static_cast<uint32_t>(entry.file_size());
            
            std::string filename = entry.path().filename().string();
            std::cout << "Processing file: " << filename << std::endl;
            // check if file is named or hashed
            if (filename.size() >= 7 && filename.rfind(".hashed") == filename.size() - 7)
            {
                bin_entry.hash = std::stoul(filename.substr(0, filename.size() - 7), nullptr, 16);
            }
            else
            {
                bin_entry.hash = str_to_hash(filename.c_str(), header.hash_a, header.hash_b);
            }

            entries.push_back(bin_entry);

            std::ifstream input_file(entry.path(), std::ios::binary);
            if (input_file)
            {
                std::vector<char> data(bin_entry.size);
                input_file.read(data.data(), bin_entry.size);
                file_dat.write(data.data(), data.size());
                input_file.close();

                // Pad to 0x800 block size
                size_t padding_size = 0x800 - (bin_entry.size % 0x800);
                if (padding_size != 0x800)
                {
                    std::vector<char> padding(padding_size, 0);
                    file_dat.write(padding.data(), padding.size());
                }
            }
            else
            {
                std::cerr << "Failed to read " << entry.path() << std::endl;
            }
        }
    }

    // Sort entries by hash value in ascending order
    std::sort(entries.begin(), entries.end(), [](const dvd_bin_entry_t &a, const dvd_bin_entry_t &b) {
        return a.hash < b.hash;
    });

    header.file_count = static_cast<uint32_t>(entries.size());
    file_bin.write(reinterpret_cast<const char *>(&header), sizeof(header));
    file_bin.write(reinterpret_cast<const char *>(entries.data()), sizeof(dvd_bin_entry_t) * header.file_count);

    file_bin.close();
    file_dat.close();
    std::cout << "Rebuilding completed." << std::endl;
}

void import_dvd(const char *dvd_bin, const char *dvd_dat, const char *input_file)
{
    std::ifstream file_bin(dvd_bin, std::ios::binary);
    if (!file_bin)
    {
        std::cerr << "Failed to open " << dvd_bin << std::endl;
        return;
    }

    std::fstream file_dat(dvd_dat, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_dat)
    {
        std::cerr << "Failed to open " << dvd_dat << std::endl;
        return;
    }

    dvd_bin_header_t header;
    file_bin.read(reinterpret_cast<char *>(&header), sizeof(dvd_bin_header_t));

    std::vector<dvd_bin_entry_t> entries(header.file_count);
    file_bin.read(reinterpret_cast<char *>(entries.data()), sizeof(dvd_bin_entry_t) * header.file_count);

    // Check if file is named or hashed
    std::string filename = std::filesystem::path(input_file).filename().string();
    uint32_t hash = 0;
    if (filename.size() >= 7 && filename.rfind(".hashed") == filename.size() - 7)
    {
        hash = std::stoul(filename.substr(0, filename.size() - 7), nullptr, 16);
    }
    else
    {
        hash = str_to_hash(filename.c_str(), header.hash_a, header.hash_b);
    }

    auto it = std::find_if(entries.begin(), entries.end(), [hash](const dvd_bin_entry_t &entry) {
        return entry.hash == hash;
    });

    if (it != entries.end())
    {
        std::cout << "Importing file: " << filename << ", at offset: " << std::hex << it->offset << std::dec << std::endl;

        file_dat.seekp(it->offset, std::ios_base::beg);
        std::ifstream input_file_stream(input_file, std::ios::binary);
        if (input_file_stream)
        {
            input_file_stream.seekg(0, std::ios::end);
            size_t input_file_size = input_file_stream.tellg();
            if (input_file_size > it->size)
            {
                std::cerr << "Input file is larger than original size." << " Use the rebuild option instead." << std::endl;
                return;
            }
            input_file_stream.seekg(0, std::ios::beg);
            std::vector<char> data(input_file_size);
            input_file_stream.read(data.data(), input_file_size);
            file_dat.write(data.data(), input_file_size);

            // Pad with zeros if the input file is smaller than the original size
            size_t padding_size = it->size - input_file_size;
            if (padding_size > 0)
            {
                std::vector<char> padding(padding_size, 0);
                file_dat.write(padding.data(), padding_size);
            }

            input_file_stream.close();
        }
        else
        {
            std::cerr << "Failed to read " << input_file << std::endl;
        }
    }
    else
    {
        std::cerr << "Hash not found in DVD entries: " << std::hex << hash << std::endl;
    }
    file_bin.close();
    file_dat.close();
    std::cout << "Import completed." << std::endl;
}