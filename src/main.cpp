#include "dvd.hpp"
#include "datafile.hpp"
#include "elf_decrypt.hpp"
#include "iso.hpp"
#include <string>
#include <iostream>
#include <unordered_map>
#include <functional>

void usage(const char* program)
{
    std::cout << std::endl;
    std::cout << "Usage: " << std::filesystem::path(program).filename().string() << " [cmd] [file|text]\n";
    std::cout << std::endl;
    std::cout << "Hash Commands:\n";
    std::cout << "  -hash [text]  Hash text\n";
    std::cout << "  -name [hash]  Get name from hash\n";
    std::cout << std::endl;
    std::cout << "DVD File Commands:\n";
    std::cout << "  -udvd [DVD.BIN] [DVD.DAT] [out dir] [--evo] Unpack DVD files (--evo also unpacks EVO data files)\n";
    std::cout << "  -rdvd [DVD.BIN] [DVD.DAT] [input dir] Build new DVD files\n";
    std::cout << "  -idvd [DVD.BIN] [DVD.DAT] [input file] Reimports a file (can't be bigger than original)\n";
    std::cout << std::endl;
    std::cout << "EVO Datafile Commands:\n";
    std::cout << "  -udat [datafile] [out dir] Unpack EVO data file\n";
    std::cout << "  -rdat [decompressed file] [out dat] Rebuild EVO data file (generic RAW0)\n";
    std::cout << std::endl;
    std::cout << "PS2 ELF Commands:\n";
    std::cout << "  -delf [packed ELF] [out ELF] [--packed-vaddr N]\n";
    std::cout << "        [--scratchpad-memsz N] [--eflags N]\n";
    std::cout << "        Decompress a self-extracting WRC ELF\n";
    std::cout << "  -diso [in ISO] [out ISO] [--name PATH]\n";
    std::cout << "        [--scratchpad-memsz N] [--eflags N]\n";
    std::cout << "        Write a copy of the ISO that boots the decompressed ELF\n";
    std::cout << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::unordered_map<std::string, std::function<int(int, char**)>> commands;
    // Hash commands
    commands["-hash"] = [](int argc, char** argv) -> int {
        if (argc < 3) {
            std::cerr << "Missing text argument for -h\n";
            return 1;
        }
        uint32_t hash = str_to_hash(argv[2], 0x7AB7, 0x6A2F);
        std::cout << "Hash: " << std::hex << hash << std::endl;
        return 0;
    };
    commands["-name"] = [](int argc, char** argv) -> int {
        if (argc < 3) {
            std::cerr << "Missing hash argument for -n\n";
            return 1;
        }
        uint32_t hash = std::stoul(argv[2], nullptr, 16);
        std::cout << "Filename: " << hash_to_name(hash) << std::endl;
        return 0;
    };

    // DVD File commands
    commands["-udvd"] = [](int argc, char** argv) -> int {
        if (argc < 5) {
            std::cerr << "Usage: -udvd [DVD.BIN] [DVD.DAT] [out dir] [--evo]\n";
            return 1;
        }
        bool evo = false;
        for (int i = 5; i < argc; ++i) {
            if (std::string(argv[i]) == "--evo")
                evo = true;
            else {
                std::cerr << "unknown option: " << argv[i] << "\n";
                return 1;
            }
        }
        unpack_dvd(argv[2], argv[3], argv[4], evo);
        return 0;
    };

    commands["-rdvd"] = [](int argc, char** argv) -> int {
        if (argc < 5) {
            std::cerr << "Usage: -rdvd [DVD.BIN] [DVD.DAT] [input dir]\n";
            return 1;
        }
        rebuild_dvd(argv[2], argv[3], argv[4]);
        return 0;
    };

    commands["-idvd"] = [](int argc, char** argv) -> int {
        if (argc < 5) {
            std::cerr << "Usage: -idvd [DVD.BIN] [DVD.DAT] [input file]\n";
            return 1;
        }
        import_dvd(argv[2], argv[3], argv[4]);
        return 0;
    };

    // EVO Datafile commands
    commands["-udat"] = [](int argc, char** argv) -> int {
        if (argc < 4) {
            std::cerr << "Usage: -udat [datafile] [out dir]\n";
            return 1;
        }
        unpack_datafile(argv[2], argv[3]);
        return 0;
    };

    commands["-rdat"] = [](int argc, char** argv) -> int {
        if (argc < 4) {
            std::cerr << "Usage: -rdat [decompressed file] [out dat]\n";
            return 1;
        }
        rebuild_datafile(argv[2], argv[3]);
        return 0;
    };

    // PS2 ELF commands
    commands["-delf"] = [](int argc, char** argv) -> int {
        try {
            return elfdecrypt::command_decompress_elf(argc, argv);
        } catch (const std::exception &e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    };
    commands["-diso"] = [](int argc, char** argv) -> int {
        try {
            return isopatch::command_patch_iso(argc, argv);
        } catch (const std::exception &e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    };

    std::string cmd = argv[1];
    auto it = commands.find(cmd);
    if (it != commands.end()) {
        return it->second(argc, argv);
    } else {
        usage(argv[0]);
        return 1;
    }
}