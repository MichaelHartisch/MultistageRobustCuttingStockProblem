#include "InstanceWriter.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct ModelSelection {
    bool qlp = true;
    bool ro = true;
    bool ro_rc = true;
    bool mip = true;
    bool rc = true;
};

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " instance.raw [output_base] [options]\n\n"
        << "Reads an existing .raw/.info file and writes selected non-CPLEX model files.\n"
        << "By default, all supported model files are created. Use --no-* flags to skip\n"
        << "large files that are not needed for a particular experiment.\n\n"
        << "Options:\n"
        << "  --no-qlp       Do not create the QIP model file.\n"
        << "  --no-ro        Do not create the RO model file.\n"
        << "  --no-ro-rc     Do not create the RO_RC model file.\n"
        << "  --no-mip       Do not create the MIP model file.\n"
        << "  --no-rc        Do not create the RC model file.\n"
        << "  --help         Show this message.\n\n"
        << "Notes:\n"
        << "  The old MIP-as-QLP writer is intentionally not generated anymore.\n"
        << "  Use GenerateInstances to create .raw files only.\n";
}

bool isOption(const std::string& value) {
    return value.rfind("--", 0) == 0;
}

void applyOption(const std::string& option, ModelSelection& selection) {
    if (option == "--no-qlp") {
        selection.qlp = false;
    } else if (option == "--no-ro") {
        selection.ro = false;
    } else if (option == "--no-ro-rc") {
        selection.ro_rc = false;
    } else if (option == "--no-mip") {
        selection.mip = false;
    } else if (option == "--no-rc") {
        selection.rc = false;
    } else {
        throw std::runtime_error("Unknown option: " + option);
    }
}
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    try {
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--help") {
                usage(argv[0]);
                return 0;
            }
        }

        const std::string infoFile = argv[1];
        std::string outputBase;
        ModelSelection selection;

        int argIndex = 2;
        if (argIndex < argc && !isOption(argv[argIndex])) {
            outputBase = argv[argIndex];
            ++argIndex;
        }

        for (; argIndex < argc; ++argIndex) {
            applyOption(argv[argIndex], selection);
        }

        InstanceWriter writer = InstanceWriter::FromInfoFile(infoFile, outputBase);

        bool wroteAny = false;

        if (selection.qlp) {
            writer.writeQIP();
            wroteAny = true;
        }
        if (selection.ro) {
            writer.writeRO();
            wroteAny = true;
        }
        if (selection.ro_rc) {
            writer.writeRO_RC();
            wroteAny = true;
        }
        if (selection.mip) {
            writer.writeMIP();
            wroteAny = true;
        }
        if (selection.rc) {
            writer.writeRC();
            wroteAny = true;
        }

        if (!wroteAny) {
            std::cerr << "No model files requested; all model writers were skipped.\n";
            return 0;
        }

//        std::cerr << "Wrote selected model files for " << writer.getBaseName() << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        usage(argv[0]);
        return 1;
    }
    return 0;
}
