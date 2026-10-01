#include "InstanceWriter.h"

#include "CplexSolver.h"
#include "DR_Holder.h"
#include "ScenarioEncoding.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kDecisionRuleEpsilon = 1.0;

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

std::string aTopName(const std::string& scenarioPrefix, int bar, int item) {
    return "aT_" + scenarioPrefix + "_" + std::to_string(bar) + "_" + std::to_string(item);
}

std::string aBottomName(const std::string& scenarioPrefix, int bar, int item) {
    return "aB_" + scenarioPrefix + "_" + std::to_string(bar) + "_" + std::to_string(item);
}

std::string zTopName(const std::string& scenarioPrefix, int bar, int item, int piece) {
    return "zT_" + scenarioPrefix + "_" + std::to_string(bar) + "_" +
           std::to_string(item) + "_" + std::to_string(piece);
}

std::string zBottomName(const std::string& scenarioPrefix, int bar, int item, int piece) {
    return "zB_" + scenarioPrefix + "_" + std::to_string(bar) + "_" +
           std::to_string(item) + "_" + std::to_string(piece);
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

void addTerm(CplexSolver& solver,
             std::vector<int>& indices,
             std::vector<double>& coefficients,
             const std::string& variableName,
             double coefficient) {
    indices.push_back(solver.RequireVariableIndex(variableName));
    coefficients.push_back(coefficient);
}

/*
 * Adds the linear threshold expression
 *
 *     yTc + sum_{recent history} yT_h
 *
 * for the P=1 rule.
 */
void addLinearDecisionRuleExpression(CplexSolver& solver,
                                     std::vector<int>& indices,
                                     std::vector<double>& coefficients,
                                     const std::string& scenarioPrefix,
                                     int stage,
                                     int consideredStages,
                                     int numberOfItems,
                                     int bar,
                                     int item,
                                     int piece,
                                     double sign) {
    const int firstRelevantStage = std::max(0, stage - consideredStages);

    for (int previousStage = firstRelevantStage; previousStage < stage; ++previousStage) {
        for (int previousItem = 0; previousItem < numberOfItems; ++previousItem) {
            if (ScenarioEncoding::scenarioItemEquals(
                    scenarioPrefix,
                    static_cast<std::size_t>(previousStage),
                    previousItem)) {
                const int historyIndex = previousStage * numberOfItems + previousItem;
                addTerm(solver,
                        indices,
                        coefficients,
                        yTopName(stage - 1, bar, item, piece, historyIndex),
                        sign);
            }
        }
    }

    addTerm(solver,
            indices,
            coefficients,
            yTopConstantName(stage - 1, bar, item, piece),
            sign);
}

/*
 * Adds either yTop_p(xi) or yBottom_p(xi) to a row.
 */
void addPieceExpression(CplexSolver& solver,
                        std::vector<int>& indices,
                        std::vector<double>& coefficients,
                        const std::string& scenarioPrefix,
                        int stage,
                        int consideredStages,
                        int numberOfItems,
                        int bar,
                        int item,
                        int piece,
                        bool top,
                        double sign) {
    const int firstRelevantStage = std::max(0, stage - consideredStages);

    for (int previousStage = firstRelevantStage; previousStage < stage; ++previousStage) {
        for (int previousItem = 0; previousItem < numberOfItems; ++previousItem) {
            if (ScenarioEncoding::scenarioItemEquals(
                    scenarioPrefix,
                    static_cast<std::size_t>(previousStage),
                    previousItem)) {
                const int historyIndex = previousStage * numberOfItems + previousItem;
                addTerm(solver,
                        indices,
                        coefficients,
                        top ? yTopName(stage - 1, bar, item, piece, historyIndex)
                            : yBottomName(stage - 1, bar, item, piece, historyIndex),
                        sign);
            }
        }
    }

    addTerm(solver,
            indices,
            coefficients,
            top ? yTopConstantName(stage - 1, bar, item, piece)
                : yBottomConstantName(stage - 1, bar, item, piece),
            sign);
}

} // namespace

/*
 * Add all master constraints that are needed for newly added scenario-tree
 * nodes.
 *
 * This replaces the old DR_prepare() + AddAllNewRows() mechanism. The old
 * version filled Solver->LazyRows and then added them to CPLEX in a separate
 * call. This version adds the corresponding rows directly to the supplied
 * CplexSolver object.
 *
 * The function assumes that BuildInitialDecisionRuleMaster(...) has already
 * created all global variables and all scenario-indexed variables over the full
 * scenario tree. For P > 1, this function creates the scenario-specific
 * auxiliary max variables aT/aB/zT/zB when the corresponding node first enters
 * the partial tree.
 */
void InstanceWriter::AddDecisionRuleMasterRowsForNewScenarios(
    CplexSolver& solver,
    const DR_Holder& decisionRule)
{
    (void)decisionRule;

    Solver = &solver;

    const bool usePiecewiseRule = (P > 1);
    const double INF = CPX_INFBOUND;

    /*
     * For P > 1, create the auxiliary variables associated with newly added
     * non-root scenario prefixes.
     */
    if (usePiecewiseRule) {
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            const int level = stage - 1;

            for (std::size_t s = 0; s < STree.SubScenarios[level].size(); ++s) {
                const int nodeIndex = STree.SubScenariosToNode[level][s];
                if (!STree.CurrentTree[nodeIndex].IsNew) {
                    continue;
                }

                const std::string& prefix = STree.SubScenarios[level][s];

                for (int b = 0; b < NumberOfBars; ++b) {
                    for (int i = 0; i < NumberOfItems; ++i) {
                        solver.AddContinuousVariable(aTopName(prefix, b, i), -INF, INF);
                        solver.AddContinuousVariable(aBottomName(prefix, b, i), -INF, INF);

                        for (int p = 0; p < P; ++p) {
                            solver.AddBinaryVariable(zTopName(prefix, b, i, p));
                            solver.AddBinaryVariable(zBottomName(prefix, b, i, p));
                        }
                    }
                }
            }
        }
    }

    /*
     * Complete-scenario constraints.
     */
    for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s) {
        const int nodeIndex = STree.SubScenariosToNode.back()[s];
        if (!STree.CurrentTree[nodeIndex].IsNew) {
            continue;
        }

        const std::string& scenario = STree.SubScenarios.back()[s];

        /*
         * If a bar is used, activate f.
         *
         *     sum_t used_{t,b} - T f_{s,b} <= 0
         */
        for (int b = 0; b < NumberOfBars; ++b) {
            std::vector<int> indices;
            std::vector<double> coefficients;

            addTerm(solver, indices, coefficients, aiName(b), 1.0);

            for (int stage = 1; stage < NumberOfStages; ++stage) {
                const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));
                for (int i = 0; i < NumberOfItems; ++i) {
                    addTerm(solver, indices, coefficients, aName(prefix, b, i), 1.0);
                }
            }

            addTerm(solver, indices, coefficients, fName(scenario, b), -NumberOfStages);

            solver.AddLinearConstraint("link_used_bar_" + scenario + "_" + std::to_string(b),
                                       indices,
                                       coefficients,
                                       'L',
                                       0.0);
        }

        /*
         * Capacity upper bound:
         *
         *     used_length + r <= bar_size
         */
        for (int b = 0; b < NumberOfBars; ++b) {
            std::vector<int> indices;
            std::vector<double> coefficients;

            addTerm(solver, indices, coefficients, aiName(b), LengthOfInitial);

            for (int stage = 1; stage < NumberOfStages; ++stage) {
                const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));
                for (int i = 0; i < NumberOfItems; ++i) {
                    addTerm(solver, indices, coefficients, aName(prefix, b, i), LengthOfItems[i]);
                }
            }

            addTerm(solver, indices, coefficients, rName(scenario, b), 1.0);

            solver.AddLinearConstraint("capacity_upper_" + scenario + "_" + std::to_string(b),
                                       indices,
                                       coefficients,
                                       'L',
                                       BarSizes[b]);
        }

        /*
         * Rest/no-rest linking constraints.
         */
        for (int b = 0; b < NumberOfBars; ++b) {
            {
                std::vector<int> indices;
                std::vector<double> coefficients;

                addTerm(solver, indices, coefficients, aiName(b), LengthOfInitial);

                for (int stage = 1; stage < NumberOfStages; ++stage) {
                    const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));
                    for (int i = 0; i < NumberOfItems; ++i) {
                        addTerm(solver, indices, coefficients, aName(prefix, b, i), LengthOfItems[i]);
                    }
                }

                addTerm(solver, indices, coefficients, rName(scenario, b), 1.0);
                addTerm(solver, indices, coefficients, nrName(scenario, b), BarSizes[b]);

                solver.AddLinearConstraint("rest_lower_" + scenario + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'G',
                                           BarSizes[b]);
            }

            {
                std::vector<int> indices;
                std::vector<double> coefficients;

                addTerm(solver, indices, coefficients, aiName(b), LengthOfInitial);

                for (int stage = 1; stage < NumberOfStages; ++stage) {
                    const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));
                    for (int i = 0; i < NumberOfItems; ++i) {
                        addTerm(solver, indices, coefficients, aName(prefix, b, i), LengthOfItems[i]);
                    }
                }

                addTerm(solver, indices, coefficients, nrName(scenario, b), MinimumRest);

                solver.AddLinearConstraint("rest_upper_" + scenario + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'L',
                                           BarSizes[b]);
            }

            {
                std::vector<int> indices;
                std::vector<double> coefficients;

                addTerm(solver, indices, coefficients, rName(scenario, b), 1.0);
                addTerm(solver, indices, coefficients, nrName(scenario, b), MinimumRest);

                solver.AddLinearConstraint("rest_cap_" + scenario + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'L',
                                           MinimumRest);
            }
        }

        /*
         * At each stage, exactly the revealed item type is assigned to one bar.
         * All non-revealed item-type assignment variables are fixed to zero.
         */
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));

            for (int i = 0; i < NumberOfItems; ++i) {
                std::vector<int> indices;
                std::vector<double> coefficients;

                for (int b = 0; b < NumberOfBars; ++b) {
                    addTerm(solver, indices, coefficients, aName(prefix, b, i), 1.0);
                }

                const double rhs =
                    ScenarioEncoding::scenarioItemEquals(
                        scenario,
                        static_cast<std::size_t>(stage - 1),
                        i)
                        ? 1.0
                        : 0.0;

                solver.AddLinearConstraint("assign_revealed_" + prefix + "_" + std::to_string(i),
                                           indices,
                                           coefficients,
                                           'E',
                                           rhs);
            }
        }

        /*
         * Switching variables.
         */
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            const std::string prefix = scenario.substr(0, static_cast<std::size_t>(stage));

            for (int b = 0; b < NumberOfBars; ++b) {
                std::vector<int> indices;
                std::vector<double> coefficients;

                for (int i = 0; i < NumberOfItems; ++i) {
                    addTerm(solver, indices, coefficients, aName(prefix, b, i), 1.0);
                }

                if (stage == 1) {
                    addTerm(solver, indices, coefficients, aiName(b), -1.0);
                } else {
                    const std::string previousPrefix =
                        scenario.substr(0, static_cast<std::size_t>(stage - 1));

                    for (int i = 0; i < NumberOfItems; ++i) {
                        addTerm(solver, indices, coefficients, aName(previousPrefix, b, i), -1.0);
                    }
                }

                addTerm(solver, indices, coefficients, wName(prefix, b), -1.0);

                solver.AddLinearConstraint("switching_" + prefix + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'L',
                                           0.0);
            }
        }
    }

    /*
     * Decision-rule constraints for newly added non-root prefixes.
     */
    for (int stage = 1; stage < NumberOfStages; ++stage) {
        const int level = stage - 1;

        for (std::size_t s = 0; s < STree.SubScenarios[level].size(); ++s) {
            const int nodeIndex = STree.SubScenariosToNode[level][s];
            if (!STree.CurrentTree[nodeIndex].IsNew) {
                continue;
            }

            const std::string& prefix = STree.SubScenarios[level][s];

            for (int b = 0; b < NumberOfBars; ++b) {
                for (int i = 0; i < NumberOfItems; ++i) {
                    const int assignmentIndex =
                        solver.RequireVariableIndex(aName(prefix, b, i));

                    /*
                     * a = 1  =>  rule expression <= 0
                     */
                    {
                        std::vector<int> indices;
                        std::vector<double> coefficients;

                        if (usePiecewiseRule) {
                            addTerm(solver, indices, coefficients, aTopName(prefix, b, i), 1.0);
                            addTerm(solver, indices, coefficients, aBottomName(prefix, b, i), -1.0);
                        } else {
                            addLinearDecisionRuleExpression(solver,
                                                            indices,
                                                            coefficients,
                                                            prefix,
                                                            stage,
                                                            ConsideredStages,
                                                            NumberOfItems,
                                                            b,
                                                            i,
                                                            0,
                                                            1.0);
                        }

                        solver.AddIndicatorConstraint(
                            "dr_assign_one_" + prefix + "_" + std::to_string(b) + "_" + std::to_string(i),
                            assignmentIndex,
                            0,
                            indices,
                            coefficients,
                            'L',
                            0.0);
                    }

                    /*
                     * a = 0  =>  rule expression >= epsilon
                     */
                    {
                        std::vector<int> indices;
                        std::vector<double> coefficients;

                        if (usePiecewiseRule) {
                            addTerm(solver, indices, coefficients, aTopName(prefix, b, i), 1.0);
                            addTerm(solver, indices, coefficients, aBottomName(prefix, b, i), -1.0);
                        } else {
                            addLinearDecisionRuleExpression(solver,
                                                            indices,
                                                            coefficients,
                                                            prefix,
                                                            stage,
                                                            ConsideredStages,
                                                            NumberOfItems,
                                                            b,
                                                            i,
                                                            0,
                                                            1.0);
                        }

                        solver.AddIndicatorConstraint(
                            "dr_assign_zero_" + prefix + "_" + std::to_string(b) + "_" + std::to_string(i),
                            assignmentIndex,
                            1,
                            indices,
                            coefficients,
                            'G',
                            kDecisionRuleEpsilon);
                    }
                }
            }
        }
    }

    /*
     * Piecewise max representation for P > 1.
     */
    if (usePiecewiseRule) {
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            const int level = stage - 1;

            for (std::size_t s = 0; s < STree.SubScenarios[level].size(); ++s) {
                const int nodeIndex = STree.SubScenariosToNode[level][s];
                if (!STree.CurrentTree[nodeIndex].IsNew) {
                    continue;
                }

                const std::string& prefix = STree.SubScenarios[level][s];

                for (int b = 0; b < NumberOfBars; ++b) {
                    for (int i = 0; i < NumberOfItems; ++i) {
                        for (int p = 0; p < P; ++p) {
                            /*
                             * aT >= yTop_p(xi)
                             */
                            {
                                std::vector<int> indices;
                                std::vector<double> coefficients;

                                addTerm(solver, indices, coefficients, aTopName(prefix, b, i), 1.0);
                                addPieceExpression(solver,
                                                   indices,
                                                   coefficients,
                                                   prefix,
                                                   stage,
                                                   ConsideredStages,
                                                   NumberOfItems,
                                                   b,
                                                   i,
                                                   p,
                                                   true,
                                                   -1.0);

                                solver.AddLinearConstraint(
                                    "top_max_lower_" + prefix + "_" +
                                        std::to_string(b) + "_" + std::to_string(i) + "_" + std::to_string(p),
                                    indices,
                                    coefficients,
                                    'G',
                                    0.0);
                            }

                            /*
                             * aB >= yBottom_p(xi)
                             */
                            {
                                std::vector<int> indices;
                                std::vector<double> coefficients;

                                addTerm(solver, indices, coefficients, aBottomName(prefix, b, i), 1.0);
                                addPieceExpression(solver,
                                                   indices,
                                                   coefficients,
                                                   prefix,
                                                   stage,
                                                   ConsideredStages,
                                                   NumberOfItems,
                                                   b,
                                                   i,
                                                   p,
                                                   false,
                                                   -1.0);

                                solver.AddLinearConstraint(
                                    "bottom_max_lower_" + prefix + "_" +
                                        std::to_string(b) + "_" + std::to_string(i) + "_" + std::to_string(p),
                                    indices,
                                    coefficients,
                                    'G',
                                    0.0);
                            }

                            /*
                             * zT_p = 1 => aT <= yTop_p(xi)
                             */
                            {
                                std::vector<int> indices;
                                std::vector<double> coefficients;

                                addTerm(solver, indices, coefficients, aTopName(prefix, b, i), 1.0);
                                addPieceExpression(solver,
                                                   indices,
                                                   coefficients,
                                                   prefix,
                                                   stage,
                                                   ConsideredStages,
                                                   NumberOfItems,
                                                   b,
                                                   i,
                                                   p,
                                                   true,
                                                   -1.0);

                                solver.AddIndicatorConstraint(
                                    "top_max_upper_" + prefix + "_" +
                                        std::to_string(b) + "_" + std::to_string(i) + "_" + std::to_string(p),
                                    solver.RequireVariableIndex(zTopName(prefix, b, i, p)),
                                    0,
                                    indices,
                                    coefficients,
                                    'L',
                                    0.0);
                            }

                            /*
                             * zB_p = 1 => aB <= yBottom_p(xi)
                             */
                            {
                                std::vector<int> indices;
                                std::vector<double> coefficients;

                                addTerm(solver, indices, coefficients, aBottomName(prefix, b, i), 1.0);
                                addPieceExpression(solver,
                                                   indices,
                                                   coefficients,
                                                   prefix,
                                                   stage,
                                                   ConsideredStages,
                                                   NumberOfItems,
                                                   b,
                                                   i,
                                                   p,
                                                   false,
                                                   -1.0);

                                solver.AddIndicatorConstraint(
                                    "bottom_max_upper_" + prefix + "_" +
                                        std::to_string(b) + "_" + std::to_string(i) + "_" + std::to_string(p),
                                    solver.RequireVariableIndex(zBottomName(prefix, b, i, p)),
                                    0,
                                    indices,
                                    coefficients,
                                    'L',
                                    0.0);
                            }
                        }

                        /*
                         * Exactly one active top and bottom piece.
                         */
                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            for (int p = 0; p < P; ++p) {
                                addTerm(solver, indices, coefficients, zTopName(prefix, b, i, p), 1.0);
                            }

                            solver.AddLinearConstraint("top_piece_select_" + prefix + "_" +
                                                           std::to_string(b) + "_" + std::to_string(i),
                                                       indices,
                                                       coefficients,
                                                       'E',
                                                       1.0);
                        }

                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            for (int p = 0; p < P; ++p) {
                                addTerm(solver, indices, coefficients, zBottomName(prefix, b, i, p), 1.0);
                            }

                            solver.AddLinearConstraint("bottom_piece_select_" + prefix + "_" +
                                                           std::to_string(b) + "_" + std::to_string(i),
                                                       indices,
                                                       coefficients,
                                                       'E',
                                                       1.0);
                        }
                    }
                }
            }
        }
    }
}

/*
 * Compatibility guard. The internal workflow should call
 * AddDecisionRuleMasterRowsForNewScenarios(...), not DR_prepare().
 */
void InstanceWriter::DR_prepare()
{
    throw std::runtime_error(
        "DR_prepare() is deprecated. Use AddDecisionRuleMasterRowsForNewScenarios(CplexSolver&, const DR_Holder&) instead.");
}
