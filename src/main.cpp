#include "dvd.hpp"
#include <string>
#include <iostream>
#include <unordered_map>
#include <functional>

void usage(const char* program)
{
    std::cout << "Usage: " << program << " [cmd] [file|text]\n";
    std::cout << "Commands:\n";
    // std::cout << "  -u [file]  Unpack file\n";
    std::cout << "  -h [text]  Hash text\n";
    std::cout << "  -dvd [DVD.BIN] [DVD.DAT] [out dir] Unpack DVD file\n";
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::unordered_map<std::string, std::function<int(int, char**)>> commands;
    // TODO: Implement unpacking
    // commands["-u"] = [](int argc, char** argv) -> int {
    //     if (argc < 3) {
    //         std::cerr << "Missing file argument for -u\n";
    //         return 1;
    //     }
    //     
    //     std::cerr << "Unpacking not implemented yet.\n";
    //     return 1;
    // };

    commands["-h"] = [](int argc, char** argv) -> int {
        if (argc < 3) {
            std::cerr << "Missing text argument for -h\n";
            return 1;
        }
        uint32_t hash = str_to_hash(argv[2], 0x7AB7, 0x6A2F);
        std::cout << "Hash: " << std::hex << hash << std::endl;
        return 0;
    };

    commands["-dvd"] = [](int argc, char** argv) -> int {
        if (argc < 5) {
            std::cerr << "Usage: -dvd [DVD.BIN] [DVD.DAT] [out dir]\n";
            return 1;
        }
        unpack_dvd(argv[2], argv[3], argv[4]);
        return 0;
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