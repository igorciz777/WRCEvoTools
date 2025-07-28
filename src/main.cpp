#include "dvd.hpp"
//#include "datafile.hpp"
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
    std::cout << "  -udvd [DVD.BIN] [DVD.DAT] [out dir] Unpack DVD files\n";
    std::cout << "  -rdvd [DVD.BIN] [DVD.DAT] [input dir] Build new DVD files\n";
    std::cout << "  -idvd [DVD.BIN] [DVD.DAT] [input file] Reimports a file (can't be bigger than original)\n";
    std::cout << std::endl;
    // std::cout << "EVO Datafile Commands:\n";
    // std::cout << "  -udat [datafile] [out dir] Unpack EVO data file\n";
    // std::cout << "  -rdat [datafile] [input dir] Rebuild EVO data file\n";
    // std::cout << std::endl;
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
            std::cerr << "Usage: -udvd [DVD.BIN] [DVD.DAT] [out dir]\n";
            return 1;
        }
        unpack_dvd(argv[2], argv[3], argv[4]);
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
            std::cerr << "Usage: -u [datafile] [out dir]\n";
            return 1;
        }
        //unpack_datafile(argv[2], argv[3]);
        std::cout << "Unpacking completed." << std::endl;
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