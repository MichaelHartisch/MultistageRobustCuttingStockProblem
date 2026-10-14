#include "InstanceWriter.h"

#include "CplexSolver.h"
#include "ScenarioEncoding.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string aiName(int bar) {
    return "ai_" + std::to_string(bar);
}

std::string aName(const std::string& scenarioPrefix, int bar, int item) {
    return "a_" + scenarioPrefix + "_" + std::to_string(bar) + "_" + std::to_string(item);
}

std::string fName(const std::string& scenario, int bar) {
    return "f_" + scenario + "_" + std::to_string(bar);
}

std::string wName(const std::string& scenarioPrefix, int bar) {
    return "w_" + scenarioPrefix + "_" + std::to_string(bar);
}

std::string rName(const std::string& scenario, int bar) {
    return "r_" + scenario + "_" + std::to_string(bar);
}

std::string nrName(const std::string& scenario, int bar) {
    return "nr_" + scenario + "_" + std::to_string(bar);
}

std::string yTopName(int stageMinusOne, int bar, int item, int piece, int historyIndex) {
    return "yT_" + std::to_string(stageMinusOne) + "_" +
           std::to_string(bar) + "_" +
           std::to_string(item) + "_" +
           std::to_string(piece) + "_" +
           std::to_string(historyIndex);
}

std::string yTopConstantName(int stageMinusOne, int bar, int item, int piece) {
    return "yTc_" + std::to_string(stageMinusOne) + "_" +
           std::to_string(bar) + "_" +
           std::to_string(item) + "_" +
           std::to_string(piece);
}

std::string yBottomName(int stageMinusOne, int bar, int item, int piece, int historyIndex) {
    return "yB_" + std::to_string(stageMinusOne) + "_" +
           std::to_string(bar) + "_" +
           std::to_string(item) + "_" +
           std::to_string(piece) + "_" +
           std::to_string(historyIndex);
}

std::string yBottomConstantName(int stageMinusOne, int bar, int item, int piece) {
    return "yBc_" + std::to_string(stageMinusOne) + "_" +
           std::to_string(bar) + "_" +
           std::to_string(item) + "_" +
           std::to_string(piece);
}

} // namespace

/*
 * Build the initial decision-rule master directly in CPLEX.
 *
 * This replaces the old file-based routine
 *
 *     writeDR_basic();
 *     ReadProblem("<base>_DR_basic.lp");
 *
 * The initial master contains:
 *   - the objective variable obj;
 *   - all variables that are needed globally or in the objective epigraph;
 *   - the objective epigraph constraints over the full scenario tree;
 *   - the initial-item assignment constraint.
 *
 * Scenario-dependent feasibility constraints are still added incrementally by
 * AddDecisionRuleMasterRowsForNewScenarios(...), which will replace the old
 * DR_prepare() routine.
 */
void InstanceWriter::BuildInitialDecisionRuleMaster(CplexSolver& solver, double Bound)
{
    Solver = &solver;

    FullTree.BuildEntireTree(getPartition());

    solver.CreateProblem(BaseName + "_DR_master");
    solver.SetMinimize();

    const double INF = CPX_INFBOUND;

    /*
     * Objective variable. The old LP had objective "obj" and the row
     *     obj >= Bound.
     * We encode this directly through the lower bound.
     */
    solver.AddContinuousVariable("obj", Bound, INF, 1.0);

    /*
     * Initial assignment variables.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        solver.AddBinaryVariable(aiName(b));
    }

    /*
     * Assignment variables for all possible prefixes in the full tree.
     *
     * These are not all constrained initially, but adding them here keeps the
     * model structure explicit. Later incremental rows can reference these
     * variables without relying on implicit on-demand creation.
     */
    for (int t = 0; t < NumberOfStages - 1; ++t) {
        for (const std::string& scenarioPrefix : FullTree.SubScenarios[t]) {
            for (int b = 0; b < NumberOfBars; ++b) {
                for (int i = 0; i < NumberOfItems; ++i) {
                    solver.AddBinaryVariable(aName(scenarioPrefix, b, i));
                }
            }
        }
    }

    /*
     * Variables appearing in the final objective expression.
     */
    for (const std::string& scenario : FullTree.SubScenarios.back()) {
        for (int b = 0; b < NumberOfBars; ++b) {
            solver.AddContinuousVariable(rName(scenario, b), 0.0, MinimumRest);
            solver.AddBinaryVariable(fName(scenario, b));
            solver.AddBinaryVariable(nrName(scenario, b));
        }
    }

    /*
     * Bar-change variables for all non-final prefixes.
     */
    for (int t = 0; t < NumberOfStages - 1; ++t) {
        for (const std::string& scenarioPrefix : FullTree.SubScenarios[t]) {
            for (int b = 0; b < NumberOfBars; ++b) {
                solver.AddContinuousVariable(wName(scenarioPrefix, b),0.0,1.0);
            }
        }
    }

    /*
     * Decision-rule coefficient variables.
     *
     * For P = 1, only yT/yTc are needed and they represent the linear
     * threshold rule directly.
     *
     * For P > 1, we keep the Bertsimas-style yTop/yBottom representation.
     * The scenario-dependent auxiliary max variables aT/aB/zT/zB are not
     * created here; they should be created when the corresponding incremental
     * rows are added for new scenarios.
     */
    for (int t = 0; t < NumberOfStages - 1; ++t) {
        const int historyLength = NumberOfItems * (t + 1);

        for (int b = 0; b < NumberOfBars; ++b) {
            for (int i = 0; i < NumberOfItems; ++i) {
                for (int p = 0; p < P; ++p) {
                    solver.AddContinuousVariable(yTopConstantName(t, b, i, p), -INF, INF);

                    for (int h = 0; h < historyLength; ++h) {
                        solver.AddContinuousVariable(yTopName(t, b, i, p, h), -INF, INF);
                    }

                    if (P > 1) {
                        solver.AddContinuousVariable(yBottomConstantName(t, b, i, p), -INF, INF);

                        for (int h = 0; h < historyLength; ++h) {
                            solver.AddContinuousVariable(yBottomName(t, b, i, p, h), -INF, INF);
                        }
                    }
                }
            }
        }
    }

    /*
     * Objective epigraph:
     *
     *   cost(scenario) - obj <= 0
     *
     * for each complete scenario of the full tree.
     */
    const int objIndex = solver.RequireVariableIndex("obj");

    for (const std::string& scenario : FullTree.SubScenarios.back()) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int b = 0; b < NumberOfBars; ++b) {
            indices.push_back(solver.RequireVariableIndex(rName(scenario, b)));
            coefficients.push_back(CostPerUnit);
        }

        for (int b = 0; b < NumberOfBars; ++b) {
            indices.push_back(solver.RequireVariableIndex(fName(scenario, b)));
            coefficients.push_back(CostPerBar);
        }

        for (int t = 1; t < NumberOfStages; ++t) {
            const std::string prefix = scenario.substr(0, static_cast<std::size_t>(t));
            for (int b = 0; b < NumberOfBars; ++b) {
                indices.push_back(solver.RequireVariableIndex(wName(prefix, b)));
                coefficients.push_back(BarChangeCost);
            }
        }

        indices.push_back(objIndex);
        coefficients.push_back(-1.0);

        solver.AddLinearConstraint("obj_epigraph_" + scenario,
                                   indices,
                                   coefficients,
                                   'L',
                                   0.0);
    }

    /*
     * The deterministic initial item must be assigned to exactly one bar.
     */
    {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int b = 0; b < NumberOfBars; ++b) {
            indices.push_back(solver.RequireVariableIndex(aiName(b)));
            coefficients.push_back(1.0);
        }

        solver.AddLinearConstraint("assign_initial_item",
                                   indices,
                                   coefficients,
                                   'E',
                                   1.0);
    }
}

/*
 * Compatibility guard: the decision-rule workflow should no longer write the
 * basic master as an LP file. Keeping this definition makes accidental old
 * calls fail loudly instead of silently writing files again.
 */
void InstanceWriter::writeDR_basic(double)
{
    throw std::runtime_error(
        "writeDR_basic() is deprecated. Use BuildInitialDecisionRuleMaster(CplexSolver&, double) instead.");
}
