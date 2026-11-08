#pragma once

// used for elf reference stuff
// https://github.com/ps2dev/ps2sdk/blob/master/ee/elf-loader2/src/include/elf_loader_common.h

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace elfdecrypt
{

    constexpr uint32_t EE_RAM_END = 0x02000000;
    constexpr uint32_t PT_LOAD = 1;
    constexpr uint32_t DEFAULT_EFLAGS = 0x20924000;
    constexpr uint32_t ELF_HEADER_SIZE = 52;
    constexpr uint32_t ELF_PHDR_SIZE = 32;
    constexpr uint32_t ELF_IMAGE_OFFSET = 0x100;

    // elf_loader_elf32_ehdr_t in ps2sdk
    struct Elf32Ehdr
    {
        uint8_t e_ident[16];
        uint16_t e_type;
        uint16_t e_machine;
        uint32_t e_version;
        uint32_t e_entry;
        uint32_t e_phoff;
        uint32_t e_shoff;
        uint32_t e_flags;
        uint16_t e_ehsize;
        uint16_t e_phentsize;
        uint16_t e_phnum;
        uint16_t e_shentsize;
        uint16_t e_shnum;
        uint16_t e_shstrndx;
    };

    // elf_loader_elf32_phdr_t in ps2sdk
    struct Elf32Phdr
    {
        uint32_t p_type;
        uint32_t p_offset;
        uint32_t p_vaddr;
        uint32_t p_paddr;
        uint32_t p_filesz;
        uint32_t p_memsz;
        uint32_t p_flags;
        uint32_t p_align;
    };

    static_assert(sizeof(Elf32Ehdr) == ELF_HEADER_SIZE, "unexpected ELF32 header size");
    static_assert(sizeof(Elf32Phdr) == ELF_PHDR_SIZE, "unexpected ELF32 program header size");

    struct StreamHeader
    {
        uint32_t dest;
        uint32_t size;
        uint32_t entry;
        uint32_t memsz;
        uint32_t file_offset;
        uint32_t vaddr;
    };

    inline std::vector<uint8_t> read_file(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("failed to open " + path.string());

        file.seekg(0, std::ios::end);
        const std::streamoff length = file.tellg();
        if (length < 0)
            throw std::runtime_error("failed to get size of " + path.string());
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(static_cast<size_t>(length));
        if (!data.empty() && !file.read(reinterpret_cast<char *>(data.data()), length))
            throw std::runtime_error("failed to read " + path.string());
        return data;
    }

    template <typename T>
    inline T load_le(const uint8_t *ptr)
    {
        T value;
        std::memcpy(&value, ptr, sizeof(value));
        return value;
    }

    inline std::vector<Elf32Phdr> parse_phdrs(const std::vector<uint8_t> &elf)
    {
        if (elf.size() < sizeof(Elf32Ehdr) || std::memcmp(elf.data(), "\x7f"
                                                                      "ELF",
                                                          4) != 0)
            throw std::runtime_error("not an ELF file");

        const Elf32Ehdr ehdr = load_le<Elf32Ehdr>(elf.data());
        if (ehdr.e_phentsize < sizeof(Elf32Phdr))
            throw std::runtime_error("invalid ELF program header size");

        const size_t table_size = static_cast<size_t>(ehdr.e_phnum) * ehdr.e_phentsize;
        if (ehdr.e_phoff > elf.size() || table_size > elf.size() - ehdr.e_phoff)
            throw std::runtime_error("ELF program header table is out of bounds");

        std::vector<Elf32Phdr> phdrs;
        phdrs.reserve(ehdr.e_phnum);
        for (uint16_t i = 0; i < ehdr.e_phnum; ++i)
        {
            const size_t offset = ehdr.e_phoff + static_cast<size_t>(i) * ehdr.e_phentsize;
            phdrs.push_back(load_le<Elf32Phdr>(elf.data() + offset));
        }
        return phdrs;
    }

    inline uint32_t normalize_ee_addr(uint32_t addr)
    {
        if (addr >= 0x20000000 && addr < 0x22000000) // uncached RAM mirror
            return addr - 0x20000000;
        if (addr >= 0x80000000 && addr < 0x82000000) // KSEG0
            return addr - 0x80000000;
        if (addr >= 0xA0000000 && addr < 0xA2000000) // KSEG1
            return addr - 0xA0000000;
        return addr;
    }

    inline bool decode_stream_header(const uint8_t *words, uint32_t &dest, uint32_t &size, uint32_t &entry)
    {
        const uint32_t dest_raw = ~load_le<uint32_t>(words);
        size = ~load_le<uint32_t>(words + 4);
        const uint32_t entry_raw = ~load_le<uint32_t>(words + 8);

        dest = normalize_ee_addr(dest_raw);
        entry = normalize_ee_addr(entry_raw);

        if ((dest & 3u) != 0 || dest < 0x100000 || dest >= EE_RAM_END)
            return false;
        if (size == 0 || size > EE_RAM_END || dest + size > EE_RAM_END)
            return false;
        if ((entry & 3u) != 0 || entry < dest || entry >= dest + size)
            return false;
        return true;
    }

    inline StreamHeader find_stream_header(const std::vector<uint8_t> &elf,
                                           std::optional<uint32_t> packed_vaddr)
    {
        const std::vector<Elf32Phdr> phdrs = parse_phdrs(elf);

        struct Candidate
        {
            uint32_t filesz;
            StreamHeader header;
        };
        std::vector<Candidate> candidates;

        auto consider = [&](const Elf32Phdr &segment, uint32_t file_offset, uint32_t vaddr)
        {
            if (file_offset > elf.size() || sizeof(uint32_t) * 3 > elf.size() - file_offset)
                return;

            uint32_t dest = 0, size = 0, entry = 0;
            if (decode_stream_header(elf.data() + file_offset, dest, size, entry))
            {
                candidates.push_back({segment.p_filesz, {dest, size, entry, 0, file_offset, vaddr}});
            }
        };

        if (packed_vaddr)
        {
            bool found = false;
            for (const Elf32Phdr &segment : phdrs)
            {
                if (segment.p_type == PT_LOAD && *packed_vaddr >= segment.p_vaddr &&
                    *packed_vaddr < segment.p_vaddr + segment.p_filesz)
                {
                    consider(segment, segment.p_offset + (*packed_vaddr - segment.p_vaddr), *packed_vaddr);
                    found = true;
                    break;
                }
            }
            if (!found || candidates.empty())
            {
                char message[128];
                std::snprintf(message, sizeof(message),
                              "no packed stream header at --packed-vaddr 0x%08x", *packed_vaddr);
                throw std::runtime_error(message);
            }
        }
        else
        {
            for (const Elf32Phdr &segment : phdrs)
            {
                if (segment.p_type == PT_LOAD && segment.p_filesz >= 12)
                    consider(segment, segment.p_offset, segment.p_vaddr);
            }

            if (candidates.empty())
            {
                for (const Elf32Phdr &segment : phdrs)
                {
                    if (segment.p_type != PT_LOAD || segment.p_filesz < 12)
                        continue;

                    const uint32_t limit = std::min<uint32_t>(segment.p_filesz, 0x1000);
                    for (uint32_t rel = 0; rel + 11 < limit; rel += 4)
                        consider(segment, segment.p_offset + rel, segment.p_vaddr + rel);
                }
            }

            if (candidates.empty())
                throw std::runtime_error(
                    "could not locate the packed stream header automatically; "
                    "pass --dest/--size/--entry explicitly, or --packed-vaddr");
        }

        const Candidate &best = *std::max_element(
            candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b)
            { return a.filesz < b.filesz; });

        StreamHeader result = best.header;

        result.memsz = result.size;
        for (const Elf32Phdr &segment : phdrs)
        {
            if (segment.p_type == PT_LOAD && normalize_ee_addr(segment.p_vaddr) == result.dest &&
                segment.p_filesz == 0 && segment.p_memsz >= result.size)
            {
                result.memsz = segment.p_memsz;
                break;
            }
        }

        return result;
    }

    inline std::vector<uint8_t> build_elf(const std::vector<uint8_t> &image, uint32_t dest,
                                          uint32_t entry, uint32_t memsz,
                                          uint32_t scratchpad_memsz = 0,
                                          uint32_t eflags = DEFAULT_EFLAGS)
    {
        const uint32_t phnum = 2 + (scratchpad_memsz > 0 ? 1 : 0);
        if (ELF_HEADER_SIZE + ELF_PHDR_SIZE * phnum > ELF_IMAGE_OFFSET)
            throw std::runtime_error("program headers do not fit before the image");

        memsz = std::max<uint32_t>(memsz, static_cast<uint32_t>(image.size()));

        std::vector<Elf32Phdr> phdrs;
        phdrs.reserve(phnum);

        phdrs.push_back({PT_LOAD, ELF_IMAGE_OFFSET, dest, dest,
                         static_cast<uint32_t>(image.size()), memsz, 7, 0x80});

        phdrs.push_back({PT_LOAD, ELF_IMAGE_OFFSET + static_cast<uint32_t>(image.size()),
                         dest + memsz, dest + memsz, 0, 0, 6, 0x10});

        if (scratchpad_memsz > 0)
        {
            // scratchpad zero-fill at 0x70000000
            phdrs.push_back({PT_LOAD, ELF_IMAGE_OFFSET + static_cast<uint32_t>(image.size()),
                             0x70000000, 0x70000000, 1, scratchpad_memsz, 6, 0x10});
        }

        Elf32Ehdr ehdr{};
        std::memcpy(ehdr.e_ident, "\x7f"
                                  "ELF",
                    4);
        ehdr.e_ident[4] = 1; // ELFCLASS32
        ehdr.e_ident[5] = 1; // ELFDATA2LSB
        ehdr.e_ident[6] = 1; // EV_CURRENT
        ehdr.e_type = 2;     // ET_EXEC
        ehdr.e_machine = 8;  // EM_MIPS
        ehdr.e_version = 1;  // EV_CURRENT
        ehdr.e_entry = entry;
        ehdr.e_phoff = ELF_HEADER_SIZE;
        ehdr.e_shoff = 0;
        ehdr.e_flags = eflags;
        ehdr.e_ehsize = ELF_HEADER_SIZE;
        ehdr.e_phentsize = ELF_PHDR_SIZE;
        ehdr.e_phnum = static_cast<uint16_t>(phnum);
        ehdr.e_shentsize = 40;
        ehdr.e_shnum = 0;
        ehdr.e_shstrndx = 0;

        std::vector<uint8_t> out(ELF_IMAGE_OFFSET, 0);
        std::memcpy(out.data(), &ehdr, sizeof(ehdr));
        for (size_t i = 0; i < phdrs.size(); ++i)
        {
            std::memcpy(out.data() + ELF_HEADER_SIZE + i * ELF_PHDR_SIZE, &phdrs[i],
                        sizeof(Elf32Phdr));
        }
        out.insert(out.end(), image.begin(), image.end());
        if (scratchpad_memsz > 0)
            out.push_back(0);
        return out;
    }

    inline int extract_decrypted_elf(const std::filesystem::path &savestate_path,
                                     const std::filesystem::path &output_path,
                                     const std::filesystem::path &packed_path = {},
                                     std::optional<uint32_t> packed_vaddr = std::nullopt,
                                     std::optional<uint32_t> dest_override = std::nullopt,
                                     std::optional<uint32_t> size_override = std::nullopt,
                                     std::optional<uint32_t> entry_override = std::nullopt,
                                     std::optional<uint32_t> memsz_override = std::nullopt,
                                     uint32_t scratchpad_memsz = 0,
                                     uint32_t eflags = DEFAULT_EFLAGS)
    {
        const std::vector<uint8_t> memory = read_file(savestate_path);
        if (memory.size() != 32u * 1024u * 1024u)
            std::cerr << "warning: eeMemory.bin is " << memory.size()
                      << " bytes, expected " << (32u * 1024u * 1024u) << std::endl;

        std::optional<StreamHeader> detected;
        if (!packed_path.empty())
        {
            const std::vector<uint8_t> packed = read_file(packed_path);
            detected = find_stream_header(packed, packed_vaddr);
            std::cout << "packed header : file offset 0x" << std::hex << detected->file_offset
                      << ", vaddr 0x" << detected->vaddr << std::dec << std::endl;
            std::cout << "detected      : dest=0x" << std::hex << detected->dest
                      << " size=0x" << detected->size
                      << " entry=0x" << detected->entry
                      << " memsz=0x" << detected->memsz << std::dec << std::endl;
        }

        if (!detected && (!dest_override || !size_override || !entry_override))
            throw std::runtime_error(
                "--dest, --size and --entry are required when --packed is not supplied");

        const uint32_t dest = dest_override ? *dest_override : detected->dest;
        const uint32_t size = size_override ? *size_override : detected->size;
        const uint32_t entry = entry_override ? *entry_override : detected->entry;
        uint32_t memsz = detected ? detected->memsz : size;
        if (memsz_override)
            memsz = *memsz_override;

        if (dest > memory.size() || size > memory.size() - dest)
        {
            throw std::runtime_error("image range 0x" + [&]
                                     {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%x..%x", dest, dest + size);
            return std::string(buffer); }() + " exceeds eeMemory.bin");
        }

        const std::vector<uint8_t> image(memory.begin() + dest, memory.begin() + dest + size);
        if (std::all_of(image.begin(), image.end(), [](uint8_t byte)
                        { return byte == 0; }))
            throw std::runtime_error("image is all zero; wrong savestate or address?");

        const std::vector<uint8_t> elf = build_elf(image, dest, entry, memsz,
                                                   scratchpad_memsz, eflags);

        std::ofstream output(output_path, std::ios::binary);
        if (!output)
            throw std::runtime_error("failed to open " + output_path.string());
        output.write(reinterpret_cast<const char *>(elf.data()),
                     static_cast<std::streamsize>(elf.size()));
        if (!output)
            throw std::runtime_error("failed to write " + output_path.string());

        std::cout << "destination   : 0x" << std::hex << dest << std::dec << std::endl;
        std::cout << "image size    : 0x" << std::hex << size << std::dec
                  << " (" << size << " bytes)" << std::endl;
        std::cout << "entry point   : 0x" << std::hex << entry << std::dec << std::endl;
        std::cout << "p_memsz       : 0x" << std::hex << std::max(memsz, size) << std::dec << std::endl;
        std::cout << "scratchpad    : 0x" << std::hex << scratchpad_memsz << std::dec << std::endl;
        std::cout << "output        : " << output_path.string() << " (" << elf.size()
                  << " bytes)" << std::endl;
        return 0;
    }

    inline uint32_t parse_number(const char *text)
    {
        if (text == nullptr || *text == '\0')
            throw std::runtime_error("empty numeric argument");

        char *end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 0);
        if (end == text || *end != '\0')
            throw std::runtime_error(std::string("invalid number: ") + text);
        return static_cast<uint32_t>(value);
    }

    inline int command_extract_decrypted_elf(int argc, char **argv)
    {
        // argv[0] = program, argv[1] = "-eelf"
        if (argc < 4)
        {
            std::cerr << "Usage: -eelf [eeMemory.bin] [out ELF] [--packed ELF] "
                         "[--packed-vaddr N] [--dest N] [--size N] [--entry N] "
                         "[--memsz N] [--scratchpad-memsz N] [--eflags N]\n";
            return 1;
        }

        std::filesystem::path savestate = argv[2];
        std::filesystem::path output = argv[3];
        std::filesystem::path packed;
        std::optional<uint32_t> packed_vaddr;
        std::optional<uint32_t> dest;
        std::optional<uint32_t> size;
        std::optional<uint32_t> entry;
        std::optional<uint32_t> memsz;
        uint32_t scratchpad_memsz = 0;
        uint32_t eflags = DEFAULT_EFLAGS;

        for (int i = 4; i < argc; ++i)
        {
            const std::string option = argv[i];
            const bool needs_value = option != "--help" && option != "-h";
            if (needs_value && i + 1 >= argc)
                throw std::runtime_error("missing value for " + option);

            if (option == "--packed" || option == "--encrypted")
                packed = argv[++i];
            else if (option == "--packed-vaddr")
                packed_vaddr = parse_number(argv[++i]);
            else if (option == "--dest")
                dest = parse_number(argv[++i]);
            else if (option == "--size")
                size = parse_number(argv[++i]);
            else if (option == "--entry")
                entry = parse_number(argv[++i]);
            else if (option == "--memsz")
                memsz = parse_number(argv[++i]);
            else if (option == "--scratchpad-memsz")
                scratchpad_memsz = parse_number(argv[++i]);
            else if (option == "--eflags")
                eflags = parse_number(argv[++i]);
            else if (option == "--help" || option == "-h")
            {
                std::cout << "Usage: -eelf [eeMemory.bin] [out ELF] [--packed ELF] "
                             "[--packed-vaddr N] [--dest N] [--size N] [--entry N] "
                             "[--memsz N] [--scratchpad-memsz N] [--eflags N]\n";
                return 0;
            }
            else
                throw std::runtime_error("unknown option: " + option);
        }

        return extract_decrypted_elf(savestate, output, packed, packed_vaddr, dest, size,
                                     entry, memsz, scratchpad_memsz, eflags);
    }

}
