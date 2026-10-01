#include "InstanceWriter.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " instance.raw [output_base]\n\n"
        << "Reads an existing .raw/.info file and writes the non-CPLEX model files.\n";
}
}

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        usage(argv[0]);
        return 1;
    }

    try {
        const std::string infoFile = argv[1];
        const std::string outputBase = (argc == 3) ? argv[2] : "";
        InstanceWriter writer = InstanceWriter::FromInfoFile(infoFile, outputBase);

        writer.writeQIP();
        writer.writeRO();
        writer.writeRO_RC();
        writer.writeMIP();
        writer.writeMIPasQLP();
        writer.writeRC();

        std::cerr << "Wrote model files for " << writer.getBaseName() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
