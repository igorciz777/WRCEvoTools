#pragma once

#include "elf_decrypt.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace isopatch
{
    constexpr uint32_t SECTOR = 2048;
    constexpr uint32_t PVD_SECTOR = 16;

    struct FileEntry
    {
        std::string path;
        uint32_t dir_lba;
        uint32_t dir_offset;
        uint32_t lba;
        uint32_t size;
        uint8_t flags;
    };

    inline uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24; }
    inline uint32_t be32(const uint8_t *p) { return p[3] | p[2] << 8 | p[1] << 16 | p[0] << 24; }

    inline void put32(uint8_t *p, uint32_t value)
    {
        p[0] = p[7] = static_cast<uint8_t>(value);
        p[1] = p[6] = static_cast<uint8_t>(value >> 8);
        p[2] = p[5] = static_cast<uint8_t>(value >> 16);
        p[3] = p[4] = static_cast<uint8_t>(value >> 24);
    }

    inline std::vector<uint8_t> read_at(std::istream &file, uint64_t offset, uint64_t size)
    {
        std::vector<uint8_t> data(static_cast<size_t>(size));
        file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("failed to read " + std::to_string(size) + " bytes at 0x" +
                                     [&]
                                     {
                                         char buffer[32];
                                         std::snprintf(buffer, sizeof(buffer), "%llx",
                                                       static_cast<unsigned long long>(offset));
                                         return std::string(buffer);
                                     }());
        return data;
    }

    inline void walk(std::ifstream &iso, uint32_t lba, uint32_t size, const std::string &path,
                     int depth, std::vector<FileEntry> &out)
    {
        if (depth > 8 || size == 0 || size > 0x1000000)
            return;

        const std::vector<uint8_t> dir = read_at(iso, static_cast<uint64_t>(lba) * SECTOR, size);
        size_t i = 0;
        while (i < dir.size())
        {
            const uint8_t length = dir[i];
            if (length == 0)
            {
                i = (i / SECTOR + 1) * SECTOR;
                continue;
            }
            if (i + length > dir.size() || length < 34)
                break;

            const uint8_t *rec = dir.data() + i;
            const uint8_t name_length = rec[32];
            if (33 + name_length > length)
                break;

            std::string name(reinterpret_cast<const char *>(rec + 33), name_length);
            const bool self = (name == "\0" && name_length == 1) || (name == "\1" && name_length == 1);
            if (!self)
            {
                const size_t semicolon = name.find(';');
                if (semicolon != std::string::npos)
                    name.resize(semicolon);

                const std::string full = path + "/" + name;
                const uint32_t entry_lba = le32(rec + 2);
                const uint32_t entry_size = le32(rec + 10);
                if (rec[25] & 2)
                    walk(iso, entry_lba, entry_size, full, depth + 1, out);
                else
                    out.push_back({full, lba, static_cast<uint32_t>(i), entry_lba, entry_size, rec[25]});
            }
            i += length;
        }
    }

    inline std::vector<FileEntry> scan(std::ifstream &iso, uint32_t &volume_sectors)
    {
        const std::vector<uint8_t> pvd = read_at(iso, static_cast<uint64_t>(PVD_SECTOR) * SECTOR, SECTOR);
        if (std::memcmp(pvd.data() + 1, "CD001", 5) != 0)
            throw std::runtime_error("not an ISO9660 image (no CD001 at sector 16)");

        volume_sectors = le32(pvd.data() + 80);
        std::vector<FileEntry> files;
        walk(iso, le32(pvd.data() + 158), le32(pvd.data() + 166), "", 0, files);
        return files;
    }

    inline std::string boot_file_name(const std::string &system_cnf)
    {
        size_t at = system_cnf.find("BOOT2");
        if (at == std::string::npos)
            throw std::runtime_error("SYSTEM.CNF has no BOOT2 line");

        std::string value = system_cnf.substr(at, system_cnf.find('\n', at) - at);
        const size_t equals = value.find('=');
        if (equals == std::string::npos)
            throw std::runtime_error("malformed BOOT2 line in SYSTEM.CNF");

        value = value.substr(equals + 1);
        const size_t slash = value.find_last_of("\\/");
        if (slash != std::string::npos)
            value = value.substr(slash + 1);

        while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == ';'))
            value.pop_back();
        const size_t version = value.find(';');
        if (version != std::string::npos)
            value.resize(version);
        return value;
    }

    inline bool same_name(const std::string &a, const std::string &b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::toupper(static_cast<unsigned char>(a[i])) !=
                std::toupper(static_cast<unsigned char>(b[i])))
                return false;
        return true;
    }

    inline int command_patch_iso(int argc, char **argv)
    {
        // argv[0] = program, argv[1] = "-diso"
        if (argc < 4)
        {
            std::cerr << "Usage: -diso [in ISO] [out ISO] [--name PATH] "
                         "[--scratchpad-memsz N] [--eflags N]\n";
            return 1;
        }

        const std::filesystem::path in_path = argv[2];
        const std::filesystem::path out_path = argv[3];
        std::string name;
        uint32_t scratchpad_memsz = 0;
        uint32_t eflags = elfdecrypt::DEFAULT_EFLAGS;

        for (int i = 4; i < argc; ++i)
        {
            const std::string option = argv[i];
            const bool needs_value = option != "--help" && option != "-h";
            if (needs_value && i + 1 >= argc)
                throw std::runtime_error("missing value for " + option);

            if (option == "--name")
                name = argv[++i];
            else if (option == "--scratchpad-memsz")
                scratchpad_memsz = elfdecrypt::parse_number(argv[++i]);
            else if (option == "--eflags")
                eflags = elfdecrypt::parse_number(argv[++i]);
            else if (option == "--help" || option == "-h")
            {
                std::cout << "Usage: -diso [in ISO] [out ISO] [--name PATH] "
                             "[--scratchpad-memsz N] [--eflags N]\n";
                return 0;
            }
            else
                throw std::runtime_error("unknown option: " + option);
        }

        if (std::filesystem::absolute(in_path).lexically_normal() ==
            std::filesystem::absolute(out_path).lexically_normal())
            throw std::runtime_error("in and out ISO are the same file; pick a different output");

        std::ifstream iso(in_path, std::ios::binary);
        if (!iso)
            throw std::runtime_error("failed to open " + in_path.string());

        uint32_t volume_sectors = 0;
        std::vector<FileEntry> files = scan(iso, volume_sectors);

        if (name.empty())
        {
            const FileEntry *cnf = nullptr;
            for (const FileEntry &entry : files)
                if (same_name(entry.path.substr(entry.path.find_last_of('/') + 1), "SYSTEM.CNF"))
                    cnf = &entry;
            if (cnf == nullptr)
                throw std::runtime_error("no SYSTEM.CNF in the ISO; pass --name");

            const std::vector<uint8_t> text =
                read_at(iso, static_cast<uint64_t>(cnf->lba) * SECTOR, std::min<uint32_t>(cnf->size, 4096));
            name = boot_file_name(std::string(text.begin(), text.end()));
        }

        const FileEntry *target = nullptr;
        for (const FileEntry &entry : files)
            if (same_name(entry.path.substr(entry.path.find_last_of('/') + 1), name))
                target = &entry;
        if (target == nullptr)
            throw std::runtime_error("boot file " + name + " not found in the ISO");

        if (target->flags & 0x80)
            throw std::runtime_error("boot file uses multiple extents; unsupported");

        std::cout << "boot file     : " << target->path << std::endl;

        const std::vector<uint8_t> packed =
            read_at(iso, static_cast<uint64_t>(target->lba) * SECTOR, target->size);
        const elfdecrypt::StreamHeader header = elfdecrypt::find_stream_header(packed, std::nullopt);

        if (scratchpad_memsz == 0)
            scratchpad_memsz = header.scratchpad_memsz;

        const uint32_t payload_offset = elfdecrypt::payload_offset_for(packed, header);
        const std::vector<uint8_t> image =
            elfdecrypt::unpack_image(iso, static_cast<uint64_t>(target->lba) * SECTOR + payload_offset,
                                     packed.size() - payload_offset, header.size);

        const std::vector<uint8_t> elf = elfdecrypt::build_elf(image, header.dest, header.entry,
                                                               header.memsz, scratchpad_memsz, eflags);
        iso.close();

        std::cout << "decompressed  : " << elf.size() << " bytes" << std::endl;
        std::cout << "copying ISO   : " << in_path.string() << " -> " << out_path.string() << std::endl;
        std::filesystem::copy_file(in_path, out_path,
                                   std::filesystem::copy_options::overwrite_existing);

        uint32_t last_end = 0;
        for (const FileEntry &entry : files)
        {
            const uint32_t end = entry.lba + (entry.size + SECTOR - 1) / SECTOR;
            last_end = std::max(last_end, end);
        }

        const uint32_t new_lba = last_end;
        const uint32_t new_sectors = static_cast<uint32_t>((elf.size() + SECTOR - 1) / SECTOR);

        std::fstream out(out_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!out)
            throw std::runtime_error("failed to open " + out_path.string());

        std::vector<uint8_t> padded(static_cast<size_t>(new_sectors) * SECTOR, 0);
        std::memcpy(padded.data(), elf.data(), elf.size());

        out.seekp(static_cast<std::streamoff>(new_lba) * SECTOR, std::ios::beg);
        out.write(reinterpret_cast<const char *>(padded.data()),
                  static_cast<std::streamsize>(padded.size()));

        std::vector<uint8_t> record =
            read_at(out, static_cast<uint64_t>(target->dir_lba) * SECTOR + target->dir_offset, 34);
        put32(record.data() + 2, new_lba);
        put32(record.data() + 10, static_cast<uint32_t>(elf.size()));

        out.seekp(static_cast<std::streamoff>(target->dir_lba) * SECTOR + target->dir_offset,
                  std::ios::beg);
        out.write(reinterpret_cast<const char *>(record.data()),
                  static_cast<std::streamsize>(record.size()));

        const uint32_t new_volume_end = new_lba + new_sectors;
        if (new_volume_end > volume_sectors)
        {
            std::vector<uint8_t> size_field(8);
            put32(size_field.data(), new_volume_end);
            out.seekp(static_cast<std::streamoff>(PVD_SECTOR) * SECTOR + 80, std::ios::beg);
            out.write(reinterpret_cast<const char *>(size_field.data()), 8);
            std::cout << "volume size   : 0x" << std::hex << volume_sectors << " -> 0x"
                      << new_volume_end << std::dec << " sectors" << std::endl;
        }

        out.close();
        if (!out)
            throw std::runtime_error("failed to write " + out_path.string());

        std::cout << "new extent    : sector " << new_lba << " (" << new_sectors << " sectors)"
                  << std::endl;
        std::cout << "output        : " << out_path.string() << std::endl;
        return 0;
    }

}
