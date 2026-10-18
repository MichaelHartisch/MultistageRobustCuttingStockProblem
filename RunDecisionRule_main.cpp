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
        << "Usage: " << program
        << " instance.raw NumP ConsStages [timelimit_seconds] \n\n" //[output_base]
        << "Reads an existing .raw/.info file and solves the decision-rule model with CPLEX.\n"
        << "The default global wall-clock time limit is 3600 seconds;\n"
        << "pass 0 as timelimit_seconds to disable it.\n";
        //<< "output_base is optional and only needed for naming debug/output artifacts.\n";
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
        const double globalTimeLimit = (argc >= 5) ? toDouble(argv[4], "timelimit_seconds") : 3600.0;
        const std::string outputBase = (argc == 6) ? argv[5] : "";

        // Include instance loading, tree construction and model construction
        // in the experiment-wide wall-clock time limit.
        const auto start = std::chrono::steady_clock::now();
        auto elapsedSeconds = [&]() {
            return std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
        };

        bool timeLimitReached = false;
        auto withinDeadline = [&]() {
            if (globalTimeLimit > 0.0 && elapsedSeconds() >= globalTimeLimit) {
                if (!timeLimitReached) {
                    std::cerr << "ABORT: TIMELIMIT REACHED\n";
                }
                timeLimitReached = true;
                return false;
            }
            return true;
        };

        struct DeadlineExceeded {};
        auto checkDeadline = [&]() {
            if (!withinDeadline()) throw DeadlineExceeded{};
        };

        auto applyRemainingTimeLimit = [&](CplexSolver& targetSolver) {
            if (!withinDeadline()) {
                return false;
            }
            // A limit of zero deliberately disables the experiment deadline.
            const double remaining = (globalTimeLimit <= 0.0)
                                         ? 0.0
                                         : globalTimeLimit - elapsedSeconds();
            if (globalTimeLimit > 0.0 && remaining <= 0.0) {
                timeLimitReached = true;
                return false;
            }
            targetSolver.SetTimeLimit(remaining);
            return true;
        };

        InstanceWriter masterBuilder =
            InstanceWriter::FromInfoFile(infoFile, outputBase, numPieces, consideredStages);

        InstanceWriter separationBuilder =
            InstanceWriter::FromInfoFile(infoFile, masterBuilder.getBaseName(), numPieces, consideredStages);

        DR_Holder decisionRule(masterBuilder.getNumberOfStages(),
                               masterBuilder.getNumberOfBars(),
                               masterBuilder.getNumberOfItems(),
                               numPieces);

        CplexSolver mainSolver(nullptr, masterBuilder.STree.SuccessorsPerStage);
        CplexSolver checkSolver(nullptr, masterBuilder.STree.SuccessorsPerStage);

        /*
         * Initialize a small scenario sample.  The master and its scenario-
         * indexed variables and rows grow only as separation finds new
         * scenarios.  Neither initialization routine enumerates the full tree.
         */
        if (withinDeadline()) {
            masterBuilder.STree.InitializePartialTree(masterBuilder.getPartition());
        }

        if (withinDeadline()) {
            // Abort initial master construction cooperatively.  A partially
            // constructed CPLEX model is never solved or certified.
            try {
                masterBuilder.BuildInitialDecisionRuleMaster(
                    mainSolver, 0.0, checkDeadline);
            } catch (const DeadlineExceeded&) {
                // withinDeadline() has already marked the run as TIMELIMIT.
            }
        }

        decisionRule.NecessaryScenarios.clear();
        double currentBound = 0.0;
        int iterationCounter = 0;

        while (withinDeadline()) {
            ++iterationCounter;

            /*
             * Add all newly required scenarios to the partial scenario tree and
             * then add the corresponding master constraints internally.
             *
             * This replaces the remaining file-based master update logic. The
             * implementation may internally reuse parts of the current
             * DR_prepare() logic, but should no longer write <base>_DR.lp.
             */
            masterBuilder.STree.FilterNewScenarios(decisionRule.NecessaryScenarios);
            masterBuilder.STree.ExpandTree(decisionRule.NecessaryScenarios);
            try {
                masterBuilder.AddDecisionRuleMasterRowsForNewScenarios(
                    mainSolver, decisionRule, checkDeadline);
            } catch (const DeadlineExceeded&) {
                break;
            }

            masterBuilder.STree.CleanTree(decisionRule.NecessaryScenarios);
            decisionRule.NecessaryScenarios.clear();

            if (!withinDeadline()) {
                break;
            }

            std::cerr << "Solve Main\n";
            mainSolver.UpdateBound(currentBound);
            // Refresh the CPLEX limit AFTER expanding and updating the master.
            if (!applyRemainingTimeLimit(mainSolver)) {
                break;
            }
            const double solvedMainObj = mainSolver.Solve(true);

            if (mainSolver.IsTimeLimit() || !withinDeadline()) {
                std::cerr << "Main DR solve reached the time limit.\n";
                timeLimitReached = true;
                break;
            }

            if (mainSolver.IsInfeasible()) {
                std::cerr << "Main DR problem is infeasible.\n";
                finalStatus = "INFEASIBLE";
                break;
            }

            if (!mainSolver.IsOptimal() || !mainSolver.HasObjectiveValue()) {
                std::cerr << "Main DR solve did not establish optimality (CPLEX status "
                          << mainSolver.GetLastSolutionStatus() << ").\n";
                finalStatus = "ERROR";
                break;
            }

            currentBound = solvedMainObj;

            const double mainBestBound = mainSolver.GetBestBound();

            std::cerr << "ObjValue of Main is " << currentBound << "\n";
            std::cerr << "BOUND_MAIN: " << mainBestBound << "\n";

            mainSolver.ExtractDR(decisionRule);

            if (!withinDeadline()) {
                break;
            }

            /*
             * Objective/worst-case separation.
             *
             * This replaces:
             *     writeDR_12a(decisionRule);
             *     ReadProblem("<base>_DR12a.lp");
             *
             * The routine should clear/rebuild checkSolver internally.
             */
            separationBuilder.BuildDecisionRuleObjectiveSeparationModel(checkSolver, decisionRule);

            std::cerr << "Solve 12a\n";

            if (!applyRemainingTimeLimit(checkSolver)) {
                break;
            }

            const double obj12a = checkSolver.Solve();

            if (checkSolver.IsTimeLimit() || !withinDeadline()) {
                std::cerr << "12a separation reached the time limit.\n";
                timeLimitReached = true;
                break;
            }
            if (!checkSolver.IsOptimal() || !checkSolver.HasObjectiveValue()) {
                std::cerr << "12a separation did not establish optimality (CPLEX status "
                          << checkSolver.GetLastSolutionStatus() << ").\n";
                finalStatus = "ERROR";
                break;
            }

            if (obj12a > currentBound && std::fabs(obj12a - currentBound) > 0.00001) {
                if (!checkSolver.ExtractScenario(decisionRule)) {
                    std::cerr << "12a is infeasible\n";
                }
            }

            /*
             * Constraint-violation separation.
             *
             * This replaces:
             *     prepareLines();
             *     writeDR_12b(...);
             *     ReadProblem("<base>_DR12b.lp");
             *
             * The new target objects should describe the constraints to check
             * without relying on LP-format strings.
             */
            const auto separationTargets =
                separationBuilder.PrepareDecisionRuleConstraintSeparationTargets(decisionRule);

            if (!withinDeadline()) {
                break;
            }

            std::cerr << "Solve 12bs\n";

            for (std::size_t i = 0; i < separationTargets.size(); ++i) {
                if (!withinDeadline()) {
                    break;
                }
                separationBuilder.BuildDecisionRuleConstraintSeparationModel(
                    checkSolver,
                    decisionRule,
                    separationTargets[i],
                    static_cast<int>(i));

                if (!applyRemainingTimeLimit(checkSolver)) {
                    break;
                }

                const double obj12b = checkSolver.Solve();

                if (checkSolver.IsTimeLimit() || !withinDeadline()) {
                    std::cerr << "12b separation reached the time limit.\n";
                    timeLimitReached = true;
                    break;
                }
                if (!checkSolver.IsOptimal() || !checkSolver.HasObjectiveValue()) {
                    std::cerr << "12b separation did not establish optimality (CPLEX status "
                              << checkSolver.GetLastSolutionStatus() << ").\n";
                    finalStatus = "ERROR";
                    break;
                }

                if (obj12b > 0.0 && std::fabs(obj12b) > 0.00001) {
                    checkSolver.ExtractScenario(decisionRule);
                }
            }

            if (timeLimitReached || finalStatus != "FINISHED") {
                break;
            }

            masterBuilder.STree.FilterNewScenarios(decisionRule.NecessaryScenarios);
            if (decisionRule.NecessaryScenarios.empty()) {
                break;
            }
        }

        // A complete iteration is not a certificate if the global deadline
        // expired during the last bookkeeping step.
        withinDeadline();
        const double elapsed = elapsedSeconds();

        if (timeLimitReached) {
            finalStatus = "TIMELIMIT";
        }

        std::cout << "TIME: " << elapsed << '\n';

        if (finalStatus == "FINISHED") {
            std::cout << "OBJ: " << currentBound << "\n";
        } else {
            std::cout << "OBJ: " << "\n";
        }

        std::cout << "SCENARIOS: " << (masterBuilder.STree.SubScenarios.empty()
                        ? 0 : masterBuilder.STree.SubScenarios.back().size()) << "\n";
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
