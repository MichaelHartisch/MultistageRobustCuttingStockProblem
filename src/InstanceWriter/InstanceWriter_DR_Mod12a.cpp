#include "InstanceWriter.h"

#include "CplexSolver.h"
#include "DR_Holder.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kDecisionRuleEpsilon = 1.0;

std::string aiName(int bar) {
    return "ai_" + std::to_string(bar);
}

std::string aName(int stageMinusOne, int bar, int item) {
    return "a_" + std::to_string(stageMinusOne + 1) + "_" +
           std::to_string(bar) + "_" + std::to_string(item);
}

std::string aTopName(int stageMinusOne, int bar, int item) {
    return "aT_" + std::to_string(stageMinusOne + 1) + "_" +
           std::to_string(bar) + "_" + std::to_string(item);
}

std::string aBottomName(int stageMinusOne, int bar, int item) {
    return "aB_" + std::to_string(stageMinusOne + 1) + "_" +
           std::to_string(bar) + "_" + std::to_string(item);
}

std::string zTopName(int stageMinusOne, int bar, int item, int piece) {
    return "zT_" + std::to_string(stageMinusOne + 1) + "_" +
           std::to_string(bar) + "_" + std::to_string(item) + "_" +
           std::to_string(piece);
}

std::string zBottomName(int stageMinusOne, int bar, int item, int piece) {
    return "zB_" + std::to_string(stageMinusOne + 1) + "_" +
           std::to_string(bar) + "_" + std::to_string(item) + "_" +
           std::to_string(piece);
}

std::string zName(int stage, int item) {
    return "z_" + std::to_string(stage) + "_" + std::to_string(item);
}

std::string fName(int bar) {
    return "f_" + std::to_string(bar);
}

std::string wName(int stageMinusOne, int bar) {
    return "w_" + std::to_string(stageMinusOne + 1) + "_" + std::to_string(bar);
}

std::string rName(int bar) {
    return "r_" + std::to_string(bar);
}

std::string nrName(int bar) {
    return "nr_" + std::to_string(bar);
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
 * Adds sum_h theta_h z_h to a row for the fixed P=1 linear threshold rule.
 *
 * The constant term is returned separately because CPLEX rows do not contain
 * constants. If the rule is
 *
 *     theta^T z + theta_0,
 *
 * then the row
 *
 *     theta^T z + theta_0 <= 0
 *
 * is added as
 *
 *     theta^T z <= -theta_0.
 */
double addFixedLinearRuleExpression(CplexSolver& solver,
                                    std::vector<int>& indices,
                                    std::vector<double>& coefficients,
                                    const DR_Holder& decisionRule,
                                    int stage,
                                    int numberOfItems,
                                    int bar,
                                    int item,
                                    double sign) {
    for (int previousStage = 0; previousStage < stage; ++previousStage) {
        for (int previousItem = 0; previousItem < numberOfItems; ++previousItem) {
            const int historyIndex = previousStage * numberOfItems + previousItem;
            const double coefficient =
                decisionRule.yTop[stage - 1][bar][item][0][historyIndex];

            if (std::fabs(coefficient) > 1e-12) {
                addTerm(solver,
                        indices,
                        coefficients,
                        zName(previousStage + 1, previousItem),
                        sign * coefficient);
            }
        }
    }

    return sign * decisionRule.yTopConst[stage - 1][bar][item][0];
}

/*
 * Adds
 *
 *     - yPiece_p(z)
 *
 * without the constant term, and returns the corresponding constant with the
 * same sign convention as used by the original LP writer.
 *
 * The old LP writer used rows of the form
 *
 *     aT - sum_h y_h z_h >= y_const
 *     aT - sum_h y_h z_h <= y_const
 *
 * for the top pieces, and analogously for bottom pieces.
 */
double addNegativeFixedPieceExpression(CplexSolver& solver,
                                       std::vector<int>& indices,
                                       std::vector<double>& coefficients,
                                       const DR_Holder& decisionRule,
                                       int stage,
                                       int numberOfItems,
                                       int bar,
                                       int item,
                                       int piece,
                                       bool top) {
    for (int previousStage = 0; previousStage < stage; ++previousStage) {
        for (int previousItem = 0; previousItem < numberOfItems; ++previousItem) {
            const int historyIndex = previousStage * numberOfItems + previousItem;

            const double coefficient =
                top ? decisionRule.yTop[stage - 1][bar][item][piece][historyIndex]
                    : decisionRule.yBot[stage - 1][bar][item][piece][historyIndex];

            if (std::fabs(coefficient) > 1e-12) {
                addTerm(solver,
                        indices,
                        coefficients,
                        zName(previousStage + 1, previousItem),
                        -coefficient);
            }
        }
    }

    return top ? decisionRule.yTopConst[stage - 1][bar][item][piece]
               : decisionRule.yBotConst[stage - 1][bar][item][piece];
}

} // namespace

/*
 * Build the old 12a separation problem directly in CPLEX.
 *
 * This replaces
 *
 *     writeDR_12a(decisionRule);
 *     checkSolver.ReadProblem("<base>_DR12a.lp");
 *
 * The 12a model maximizes the true objective value over admissible item
 * sequences while enforcing the fixed decision rule. If its objective exceeds
 * the current master value, ExtractScenario(...) can read the selected z
 * variables and add the corresponding scenario to decisionRule.NecessaryScenarios.
 */
void InstanceWriter::BuildDecisionRuleObjectiveSeparationModel(
    CplexSolver& solver,
    const DR_Holder& decisionRule)
{
    solver.CreateProblem(BaseName + "_DR12a");
    solver.SetMaximize();

    const bool usePiecewiseRule = (P > 1);

    /*
     * Variables.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        solver.AddBinaryVariable(aiName(b));
    }

    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int b = 0; b < NumberOfBars; ++b) {
            for (int i = 0; i < NumberOfItems; ++i) {
                solver.AddBinaryVariable(aName(stage - 1, b, i));
            }
        }
    }

    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int i = 0; i < NumberOfItems; ++i) {
            solver.AddBinaryVariable(zName(stage, i));
        }
    }

    for (int b = 0; b < NumberOfBars; ++b) {
        solver.AddBinaryVariable(fName(b), CostPerBar);
        solver.AddContinuousVariable(rName(b), 0.0, MinimumRest, CostPerUnit);
        solver.AddBinaryVariable(nrName(b));
    }

    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int b = 0; b < NumberOfBars; ++b) {
            solver.AddBinaryVariable(wName(stage - 1, b), BarChangeCost);
        }
    }

    if (usePiecewiseRule) {
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            for (int b = 0; b < NumberOfBars; ++b) {
                for (int i = 0; i < NumberOfItems; ++i) {
                    solver.AddContinuousVariable(aTopName(stage - 1, b, i), -1000.0, 1000.0);
                    solver.AddContinuousVariable(aBottomName(stage - 1, b, i), -1000.0, 1000.0);

                    for (int p = 0; p < P; ++p) {
                        solver.AddBinaryVariable(zTopName(stage - 1, b, i, p));
                        solver.AddBinaryVariable(zBottomName(stage - 1, b, i, p));
                    }
                }
            }
        }
    }

    /*
     * If a bar is used, f may be one. Since f has a positive objective
     * coefficient in this maximization problem, this reproduces the old row:
     *
     *     sum(assignments on bar b) - f_b >= 0.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        addTerm(solver, indices, coefficients, aiName(b), 1.0);

        for (int stage = 1; stage < NumberOfStages; ++stage) {
            for (int i = 0; i < NumberOfItems; ++i) {
                addTerm(solver, indices, coefficients, aName(stage - 1, b, i), 1.0);
            }
        }

        addTerm(solver, indices, coefficients, fName(b), -1.0);

        solver.AddLinearConstraint("used_bar_" + std::to_string(b),
                                   indices,
                                   coefficients,
                                   'G',
                                   0.0);
    }

    /*
     * Capacity.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int stage = 1; stage < NumberOfStages; ++stage) {
            for (int i = 0; i < NumberOfItems; ++i) {
                addTerm(solver, indices, coefficients,
                        aName(stage - 1, b, i),
                        LengthOfItems[i]);
            }
        }

        addTerm(solver, indices, coefficients, aiName(b), LengthOfInitial);
        addTerm(solver, indices, coefficients, rName(b), 1.0);

        solver.AddLinearConstraint("capacity_" + std::to_string(b),
                                   indices,
                                   coefficients,
                                   'L',
                                   BarSizes[b]);
    }

    /*
     * Rest/no-rest logic, copied from the old LP writer.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        {
            std::vector<int> indices;
            std::vector<double> coefficients;

            for (int stage = 1; stage < NumberOfStages; ++stage) {
                for (int i = 0; i < NumberOfItems; ++i) {
                    addTerm(solver, indices, coefficients,
                            aName(stage - 1, b, i),
                            LengthOfItems[i]);
                }
            }

            addTerm(solver, indices, coefficients, aiName(b), LengthOfInitial);
            addTerm(solver, indices, coefficients, nrName(b), -(BarSizes[b] - MinimumRest + 1));

            solver.AddLinearConstraint("rest_indicator_lower_" + std::to_string(b),
                                       indices,
                                       coefficients,
                                       'G',
                                       0.0);
        }

        {
            std::vector<int> indices;
            std::vector<double> coefficients;

            addTerm(solver, indices, coefficients, rName(b), 1.0);
            addTerm(solver, indices, coefficients, nrName(b), -MinimumRest);

            solver.AddLinearConstraint("rest_indicator_upper_" + std::to_string(b),
                                       indices,
                                       coefficients,
                                       'L',
                                       0.0);
        }
    }

    /*
     * Fix the first-stage bar selected by the master solution.
     */
    for (int b = 0; b < NumberOfBars; ++b) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        addTerm(solver, indices, coefficients, aiName(b), 1.0);

        solver.AddLinearConstraint("fix_initial_bar_" + std::to_string(b),
                                   indices,
                                   coefficients,
                                   'E',
                                   (b == decisionRule.FirstStageBar) ? 1.0 : 0.0);
    }

    /*
     * Switching variables.
     */
    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int b = 0; b < NumberOfBars; ++b) {
            {
                std::vector<int> indices;
                std::vector<double> coefficients;

                for (int i = 0; i < NumberOfItems; ++i) {
                    addTerm(solver, indices, coefficients, aName(stage - 1, b, i), 1.0);
                }

                addTerm(solver, indices, coefficients, wName(stage - 1, b), -1.0);

                solver.AddLinearConstraint("switch_current_" +
                                               std::to_string(stage) + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'G',
                                           0.0);
            }

            {
                std::vector<int> indices;
                std::vector<double> coefficients;

                if (stage == 1) {
                    addTerm(solver, indices, coefficients, aiName(b), 1.0);
                } else {
                    for (int i = 0; i < NumberOfItems; ++i) {
                        addTerm(solver,
                                indices,
                                coefficients,
                                aName(stage - 2, b, i),
                                1.0);
                    }
                }

                addTerm(solver, indices, coefficients, wName(stage - 1, b), 1.0);

                solver.AddLinearConstraint("switch_previous_" +
                                               std::to_string(stage) + "_" + std::to_string(b),
                                           indices,
                                           coefficients,
                                           'L',
                                           1.0);
            }
        }
    }

    /*
     * Fixed decision-rule implications.
     */
    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int b = 0; b < NumberOfBars; ++b) {
            for (int i = 0; i < NumberOfItems; ++i) {
                const int assignmentIndex =
                    solver.RequireVariableIndex(aName(stage - 1, b, i));

                /*
                 * a = 1  =>  rule expression <= 0
                 */
                {
                    std::vector<int> indices;
                    std::vector<double> coefficients;
                    double constant = 0.0;

                    if (usePiecewiseRule) {
                        addTerm(solver, indices, coefficients, aTopName(stage - 1, b, i), 1.0);
                        addTerm(solver, indices, coefficients, aBottomName(stage - 1, b, i), -1.0);
                    } else {
                        constant = addFixedLinearRuleExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                1.0);
                    }

                    solver.AddIndicatorConstraint(
                        "dr_one_" + std::to_string(stage) + "_" + std::to_string(b) + "_" + std::to_string(i),
                        assignmentIndex,
                        0,
                        indices,
                        coefficients,
                        'L',
                        -constant);
                }

                /*
                 * a = 0  =>  rule expression >= epsilon
                 */
                {
                    std::vector<int> indices;
                    std::vector<double> coefficients;
                    double constant = 0.0;

                    if (usePiecewiseRule) {
                        addTerm(solver, indices, coefficients, aTopName(stage - 1, b, i), 1.0);
                        addTerm(solver, indices, coefficients, aBottomName(stage - 1, b, i), -1.0);
                    } else {
                        constant = addFixedLinearRuleExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                1.0);
                    }

                    solver.AddIndicatorConstraint(
                        "dr_zero_" + std::to_string(stage) + "_" + std::to_string(b) + "_" + std::to_string(i),
                        assignmentIndex,
                        1,
                        indices,
                        coefficients,
                        'G',
                        kDecisionRuleEpsilon - constant);
                }
            }
        }
    }

    /*
     * Piecewise max representation for P > 1.
     */
    if (usePiecewiseRule) {
        for (int stage = 1; stage < NumberOfStages; ++stage) {
            for (int b = 0; b < NumberOfBars; ++b) {
                for (int i = 0; i < NumberOfItems; ++i) {
                    for (int p = 0; p < P; ++p) {
                        /*
                         * aT >= yTop_p(z)
                         */
                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            addTerm(solver, indices, coefficients, aTopName(stage - 1, b, i), 1.0);
                            const double constant =
                                addNegativeFixedPieceExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                p,
                                                                true);

                            solver.AddLinearConstraint(
                                "top_max_lower_" +
                                    std::to_string(stage) + "_" + std::to_string(b) + "_" +
                                    std::to_string(i) + "_" + std::to_string(p),
                                indices,
                                coefficients,
                                'G',
                                constant);
                        }

                        /*
                         * aB >= yBottom_p(z)
                         */
                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            addTerm(solver, indices, coefficients, aBottomName(stage - 1, b, i), 1.0);
                            const double constant =
                                addNegativeFixedPieceExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                p,
                                                                false);

                            solver.AddLinearConstraint(
                                "bottom_max_lower_" +
                                    std::to_string(stage) + "_" + std::to_string(b) + "_" +
                                    std::to_string(i) + "_" + std::to_string(p),
                                indices,
                                coefficients,
                                'G',
                                constant);
                        }

                        /*
                         * zT_p = 1 => aT <= yTop_p(z)
                         */
                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            addTerm(solver, indices, coefficients, aTopName(stage - 1, b, i), 1.0);
                            const double constant =
                                addNegativeFixedPieceExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                p,
                                                                true);

                            solver.AddIndicatorConstraint(
                                "top_max_upper_" +
                                    std::to_string(stage) + "_" + std::to_string(b) + "_" +
                                    std::to_string(i) + "_" + std::to_string(p),
                                solver.RequireVariableIndex(zTopName(stage - 1, b, i, p)),
                                0,
                                indices,
                                coefficients,
                                'L',
                                constant);
                        }

                        /*
                         * zB_p = 1 => aB <= yBottom_p(z)
                         */
                        {
                            std::vector<int> indices;
                            std::vector<double> coefficients;

                            addTerm(solver, indices, coefficients, aBottomName(stage - 1, b, i), 1.0);
                            const double constant =
                                addNegativeFixedPieceExpression(solver,
                                                                indices,
                                                                coefficients,
                                                                decisionRule,
                                                                stage,
                                                                NumberOfItems,
                                                                b,
                                                                i,
                                                                p,
                                                                false);

                            solver.AddIndicatorConstraint(
                                "bottom_max_upper_" +
                                    std::to_string(stage) + "_" + std::to_string(b) + "_" +
                                    std::to_string(i) + "_" + std::to_string(p),
                                solver.RequireVariableIndex(zBottomName(stage - 1, b, i, p)),
                                0,
                                indices,
                                coefficients,
                                'L',
                                constant);
                        }
                    }

                    /*
                     * Select exactly one top and bottom piece.
                     */
                    {
                        std::vector<int> indices;
                        std::vector<double> coefficients;

                        for (int p = 0; p < P; ++p) {
                            addTerm(solver,
                                    indices,
                                    coefficients,
                                    zTopName(stage - 1, b, i, p),
                                    1.0);
                        }

                        solver.AddLinearConstraint(
                            "select_top_piece_" +
                                std::to_string(stage) + "_" + std::to_string(b) + "_" + std::to_string(i),
                            indices,
                            coefficients,
                            'E',
                            1.0);
                    }

                    {
                        std::vector<int> indices;
                        std::vector<double> coefficients;

                        for (int p = 0; p < P; ++p) {
                            addTerm(solver,
                                    indices,
                                    coefficients,
                                    zBottomName(stage - 1, b, i, p),
                                    1.0);
                        }

                        solver.AddLinearConstraint(
                            "select_bottom_piece_" +
                                std::to_string(stage) + "_" + std::to_string(b) + "_" + std::to_string(i),
                            indices,
                            coefficients,
                            'E',
                            1.0);
                    }
                }
            }
        }
    }

    /*
     * Scenario variables: exactly one item type per stage.
     */
    for (int stage = 1; stage < NumberOfStages; ++stage) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int i = 0; i < NumberOfItems; ++i) {
            addTerm(solver, indices, coefficients, zName(stage, i), 1.0);
        }

        solver.AddLinearConstraint("one_item_per_stage_" + std::to_string(stage),
                                   indices,
                                   coefficients,
                                   'E',
                                   1.0);
    }

    /*
     * Each item type appears the prescribed number of times.
     */
    for (int i = 0; i < NumberOfItems; ++i) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int stage = 1; stage < NumberOfStages; ++stage) {
            addTerm(solver, indices, coefficients, zName(stage, i), 1.0);
        }

        solver.AddLinearConstraint("item_count_" + std::to_string(i),
                                   indices,
                                   coefficients,
                                   'E',
                                   Partitions[RandPart][i]);
    }
}

/*
 * Compatibility guard. The internal workflow should call
 * BuildDecisionRuleObjectiveSeparationModel(...), not writeDR_12a(...).
 */
void InstanceWriter::writeDR_12a(DR_Holder)
{
    throw std::runtime_error(
        "writeDR_12a() is deprecated. Use BuildDecisionRuleObjectiveSeparationModel(CplexSolver&, const DR_Holder&) instead.");
}
