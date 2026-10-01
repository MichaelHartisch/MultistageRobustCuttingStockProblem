#include "InstanceWriter.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

namespace fs = std::filesystem;

int toInt(const char* value, const char* name) {
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        std::cerr << "Invalid integer for " << name << ": " << value << "\n";
        std::exit(1);
    }
    return static_cast<int>(parsed);
}

std::string makeInstanceBaseName(
    int stages,
    int bars,
    int items,
    int seed
) {
    return "binPacking_"
        + std::to_string(stages) + "_"
        + std::to_string(bars) + "_"
        + std::to_string(items) + "_"
        + std::to_string(seed);
}

std::string makeInstanceOutputBase(
    const std::string& outputDirectory,
    int stages,
    int bars,
    int items,
    int seed
) {
    const fs::path directory(outputDirectory);
    const fs::path fileBase = makeInstanceBaseName(stages, bars, items, seed);
    return (directory / fileBase).string();
}

void usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " T B I seed output_directory\n"
        << "  " << program << " T B I first_seed last_seed output_directory\n\n"
        << "Creates .raw instance files only.\n"
        << "Files are named using the scheme:\n"
        << "  binPacking_T_B_I_seed.raw\n\n"
        << "Examples:\n"
        << "  " << program << " 5 3 4 1 instances/data\n"
        << "  " << program << " 5 3 4 1 20 instances/data\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 6 && argc != 7) {
        usage(argv[0]);
        return 1;
    }

    const int stages = toInt(argv[1], "T");
    const int bars = toInt(argv[2], "B");
    const int items = toInt(argv[3], "I");

    if (argc == 6) {
        const int seed = toInt(argv[4], "seed");
        const std::string outputDirectory = argv[5];

        const std::string base = makeInstanceOutputBase(
            outputDirectory,
            stages,
            bars,
            items,
            seed
        );

        InstanceWriter writer(
            stages,
            bars,
            items,
            seed,
            base,
            1,
            stages,
            InstanceWriter::UniformSuccessors(stages, items)
        );

        writer.Initialize();
        writer.CreateDataSheet();

        std::cerr << "Wrote " << base << ".raw\n";
        return 0;
    }

    const int firstSeed = toInt(argv[4], "first_seed");
    const int lastSeed = toInt(argv[5], "last_seed");
    const std::string outputDirectory = argv[6];

    if (lastSeed < firstSeed) {
        std::cerr << "last_seed must be >= first_seed\n";
        return 1;
    }

    for (int seed = firstSeed; seed <= lastSeed; ++seed) {
        const std::string base = makeInstanceOutputBase(
            outputDirectory,
            stages,
            bars,
            items,
            seed
        );

        InstanceWriter writer(
            stages,
            bars,
            items,
            seed,
            base,
            1,
            stages,
            InstanceWriter::UniformSuccessors(stages, items)
        );

        writer.Initialize();
        writer.CreateDataSheet();

        std::cerr << "Wrote " << base << ".raw\n";
    }

    return 0;
}