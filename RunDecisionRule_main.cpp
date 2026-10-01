#include "CplexSolver.h"
#include "DR_Holder.h"
#include "InstanceWriter.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <iomanip>

namespace {
int toInt(const char* value, const char* name) {
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0') {
        throw std::runtime_error(std::string("Invalid integer for ") + name + ": " + value);
    }
    return static_cast<int>(parsed);
}

double toDouble(const char* value, const char* name) {
    char* end = nullptr;
    double parsed = std::strtod(value, &end);
    if (end == value || *end != '\0') {
        throw std::runtime_error(std::string("Invalid number for ") + name + ": " + value);
    }
    return parsed;
}

void usage(const char* program) {
    std::cerr
        << "Usage: " << program << " instance.raw NumP ConsStages [output_base] [timelimit_seconds]\n\n"
        << "Reads an existing .raw/.info file, generates the decision-rule models,\n"
        << "and solves them with CPLEX. The default global wall-clock timelimit is 3600 seconds;\n"
        << "pass 0 as timelimit_seconds to disable it.\n";
}

std::vector<char> mutableCString(const std::string& value) {
    std::vector<char> result(value.begin(), value.end());
    result.push_back('\0');
    return result;
}
}

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5 && argc != 6) {
        usage(argv[0]);
        return 1;
    }
    std::string finalStatus = "FINISHED";
    try {
        const std::string infoFile = argv[1];
        const int numPieces = toInt(argv[2], "NumP");
        const int consideredStages = toInt(argv[3], "ConsStages");
        const std::string outputBase = (argc >= 5) ? argv[4] : "";
        const double globalTimeLimit = (argc == 6) ? toDouble(argv[5], "timelimit_seconds") : 3600.0;

        InstanceWriter writer = InstanceWriter::FromInfoFile(infoFile, outputBase, numPieces, consideredStages);
        DR_Holder decisionRule(writer.getNumberOfStages(), writer.getNumberOfBars(), writer.getNumberOfItems(), numPieces);
        CplexSolver mainSolver(nullptr, writer.STree.SuccessorsPerStage);
        CplexSolver checkSolver(nullptr, writer.STree.SuccessorsPerStage);
        writer.setCplexSolver(&mainSolver);

        writer.FullTree.BuildEntireTree(writer.getPartition());
        writer.writeDR_basic();

        std::string mainName = writer.getBaseName() + "_DR_basic.lp";
        std::string name12a = writer.getBaseName() + "_DR12a.lp";
        std::string name12b = writer.getBaseName() + "_DR12b.lp";
        std::vector<char> mainNameC = mutableCString(mainName);
        std::vector<char> name12aC = mutableCString(name12a);
        std::vector<char> name12bC = mutableCString(name12b);

        mainSolver.ReadProblem(mainNameC.data());
        writer.STree.InitializePartialTree(writer.getPartition());

        InstanceWriter writer12 = InstanceWriter::FromInfoFile(infoFile, writer.getBaseName(), numPieces, consideredStages);

        decisionRule.NecessaryScenarios.clear();
        auto start = std::chrono::high_resolution_clock::now();
        double currentBound = 0.0;
        int iterationCounter = 0;
        bool timeLimitReached = false;

        auto elapsedSeconds = [&]() {
            const auto current = std::chrono::high_resolution_clock::now();
            return std::chrono::duration<double>(current - start).count();
        };

        auto applyRemainingTimeLimit = [&](CplexSolver& targetSolver) {
            if (globalTimeLimit <= 0.0) {
                targetSolver.SetTimeLimit(0.0);
                return true;
            }
            const double remaining = globalTimeLimit - elapsedSeconds();
            if (remaining <= 0.0) {
                std::cerr << "ABORT: TIMELIMIT REACHED" << std::endl;
                timeLimitReached = true;
                return false;
            }
            std::cerr << remaining << std::endl;
            targetSolver.SetTimeLimit(remaining);
            return true;
        };

        do {
            if (!applyRemainingTimeLimit(mainSolver)) break;
            ++iterationCounter;
            writer.STree.FilterNewScenarios(decisionRule.NecessaryScenarios);
            writer.STree.ExpandTree(decisionRule.NecessaryScenarios);
            writer.DR_prepare();
            mainSolver.AddAllNewRows();
            writer.STree.CleanTree(decisionRule.NecessaryScenarios);
            decisionRule.NecessaryScenarios.clear();

            std::cerr << "Solve Main\n";
            mainSolver.UpdateBound(currentBound);

            const double solvedMainObj = mainSolver.Solve(true);

            if (mainSolver.IsInfeasible()) {
                std::cerr << "Main DR problem is infeasible.\n";
                finalStatus = "INFEASIBLE";
                break;
            }

            if (!mainSolver.HasObjectiveValue()) {
               if (mainSolver.IsTimeLimit()) {
                  std::cerr << "Main DR solve stopped without objective due to timelimit.\n";
                  finalStatus = "TIMELIMIT";
               } else {
                  std::cerr << "Main DR solve stopped without objective.\n";
                  finalStatus = "ERROR";
               }
               break;
            }

            currentBound = solvedMainObj;

            const double mainBestBound = mainSolver.GetBestBound();

            std::cerr << "ObjValue of Main is " << currentBound << "\n";
            std::cerr << "BOUND_MAIN: " << mainBestBound << "\n";
            mainSolver.ExtractDR(decisionRule);
            writer12.writeDR_12a(decisionRule);
            std::cerr << "Solve 12a\n";
            checkSolver.ReadProblem(name12aC.data());
            if (!applyRemainingTimeLimit(checkSolver)) break;
            double obj12a = checkSolver.Solve();
            if (obj12a > currentBound && std::fabs(obj12a - currentBound) > 0.00001) {
                if (!checkSolver.ExtractScenario(decisionRule)) {
                    std::cerr << "12a is infeasible\n";
                }
            }

            std::vector<std::string> lines = writer12.prepareLines();
            std::cerr << "Solve 12bs\n";
            for (std::size_t i = 0; i < lines.size(); ++i) {
                writer12.writeDR_12b(decisionRule, lines[i], static_cast<int>(i));
                checkSolver.ReadProblem(name12bC.data());
                if (!applyRemainingTimeLimit(checkSolver)) break;
                double obj12b = checkSolver.Solve();
                if (obj12b > 0 && std::fabs(obj12b) > 0.00001) {
                    checkSolver.ExtractScenario(decisionRule);
                }
            }
            if (timeLimitReached) break;
            writer.STree.FilterNewScenarios(decisionRule.NecessaryScenarios);
        } while (!timeLimitReached && !decisionRule.NecessaryScenarios.empty());

        auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> elapsed = end - start;
        std::cout << "TIME: " << elapsed.count() << '\n';
        if (timeLimitReached) finalStatus = "TIMELIMIT";
        if (finalStatus != "FINISHED") {
            std::cout << "OBJ: " << "\n";
        } else {
            std::cout << "OBJ: " << currentBound << "\n";
        }

        std::cout << "SCENARIOS: " << writer.STree.SubScenarios.back().size() << "\n";
        std::cout << "ITERATIONS: " << iterationCounter << "\n";
        std::cout << "STATUS: " << finalStatus << "\n";

        mainSolver.Clear();
        checkSolver.Clear();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
