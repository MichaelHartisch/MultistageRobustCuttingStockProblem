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

void setObj(CplexSolver& solver, const std::string& variableName, double coefficient) {
    solver.SetObjectiveCoefficient(solver.RequireVariableIndex(variableName), coefficient);
}

void addObjectiveConstant(CplexSolver& solver,
                          const std::string& name,
                          double value) {
    if (std::fabs(value) <= 1e-12) {
        return;
    }

    /*
     * CPLEX objectives can have a constant offset, but the lightweight wrapper
     * currently exposes only variable objective coefficients. A fixed variable
     * equal to one is a simple and explicit way to encode the offset.
     */
    solver.AddContinuousVariable("obj_const_" + name, 1.0, 1.0, value);
}

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

void addFixedDecisionRuleConstraints(InstanceWriter& writer,
                                     CplexSolver& solver,
                                     const DR_Holder& decisionRule) {
    const bool usePiecewiseRule = (writer.getNumberOfPieces() > 1);
    const int numberOfStages = writer.getNumberOfStages();
    const int numberOfBars = writer.getNumberOfBars();
    const int numberOfItems = writer.getNumberOfItems();
    const int numberOfPieces = writer.getNumberOfPieces();

    for (int stage = 1; stage < numberOfStages; ++stage) {
        for (int b = 0; b < numberOfBars; ++b) {
            for (int i = 0; i < numberOfItems; ++i) {
                const int assignmentIndex =
                    solver.RequireVariableIndex(aName(stage - 1, b, i));

                /*
                 * a = 1 => rule expression <= 0.
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
                                                                numberOfItems,
                                                                b,
                                                                i,
                                                                1.0);
                    }

                    solver.AddIndicatorConstraint(
                        "dr_one_" + std::to_string(stage) + "_" +
                            std::to_string(b) + "_" + std::to_string(i),
                        assignmentIndex,
                        0,
                        indices,
                        coefficients,
                        'L',
                        -constant);
                }

                /*
                 * a = 0 => rule expression >= epsilon.
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
                                                                numberOfItems,
                                                                b,
                                                                i,
                                                                1.0);
                    }

                    solver.AddIndicatorConstraint(
                        "dr_zero_" + std::to_string(stage) + "_" +
                            std::to_string(b) + "_" + std::to_string(i),
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

    if (!usePiecewiseRule) {
        return;
    }

    /*
     * Piecewise max representation for P > 1.
     */
    for (int stage = 1; stage < numberOfStages; ++stage) {
        for (int b = 0; b < numberOfBars; ++b) {
            for (int i = 0; i < numberOfItems; ++i) {
                for (int p = 0; p < numberOfPieces; ++p) {
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
                                                            numberOfItems,
                                                            b,
                                                            i,
                                                            p,
                                                            true);

                        solver.AddLinearConstraint(
                            "top_max_lower_" + std::to_string(stage) + "_" +
                                std::to_string(b) + "_" + std::to_string(i) + "_" +
                                std::to_string(p),
                            indices,
                            coefficients,
                            'G',
                            constant);
                    }

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
                                                            numberOfItems,
                                                            b,
                                                            i,
                                                            p,
                                                            false);

                        solver.AddLinearConstraint(
                            "bottom_max_lower_" + std::to_string(stage) + "_" +
                                std::to_string(b) + "_" + std::to_string(i) + "_" +
                                std::to_string(p),
                            indices,
                            coefficients,
                            'G',
                            constant);
                    }

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
                                                            numberOfItems,
                                                            b,
                                                            i,
                                                            p,
                                                            true);

                        solver.AddIndicatorConstraint(
                            "top_max_upper_" + std::to_string(stage) + "_" +
                                std::to_string(b) + "_" + std::to_string(i) + "_" +
                                std::to_string(p),
                            solver.RequireVariableIndex(zTopName(stage - 1, b, i, p)),
                            0,
                            indices,
                            coefficients,
                            'L',
                            constant);
                    }

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
                                                            numberOfItems,
                                                            b,
                                                            i,
                                                            p,
                                                            false);

                        solver.AddIndicatorConstraint(
                            "bottom_max_upper_" + std::to_string(stage) + "_" +
                                std::to_string(b) + "_" + std::to_string(i) + "_" +
                                std::to_string(p),
                            solver.RequireVariableIndex(zBottomName(stage - 1, b, i, p)),
                            0,
                            indices,
                            coefficients,
                            'L',
                            constant);
                    }
                }

                {
                    std::vector<int> indices;
                    std::vector<double> coefficients;

                    for (int p = 0; p < numberOfPieces; ++p) {
                        addTerm(solver, indices, coefficients, zTopName(stage - 1, b, i, p), 1.0);
                    }

                    solver.AddLinearConstraint(
                        "select_top_piece_" + std::to_string(stage) + "_" +
                            std::to_string(b) + "_" + std::to_string(i),
                        indices,
                        coefficients,
                        'E',
                        1.0);
                }

                {
                    std::vector<int> indices;
                    std::vector<double> coefficients;

                    for (int p = 0; p < numberOfPieces; ++p) {
                        addTerm(solver, indices, coefficients, zBottomName(stage - 1, b, i, p), 1.0);
                    }

                    solver.AddLinearConstraint(
                        "select_bottom_piece_" + std::to_string(stage) + "_" +
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

void addScenarioCompositionConstraints(InstanceWriter& writer, CplexSolver& solver) {
    const int numberOfStages = writer.getNumberOfStages();
    const int numberOfItems = writer.getNumberOfItems();

    for (int stage = 1; stage < numberOfStages; ++stage) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int i = 0; i < numberOfItems; ++i) {
            addTerm(solver, indices, coefficients, zName(stage, i), 1.0);
        }

        solver.AddLinearConstraint("one_item_per_stage_" + std::to_string(stage),
                                   indices,
                                   coefficients,
                                   'E',
                                   1.0);
    }

    for (int i = 0; i < numberOfItems; ++i) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        for (int stage = 1; stage < numberOfStages; ++stage) {
            addTerm(solver, indices, coefficients, zName(stage, i), 1.0);
        }

        solver.AddLinearConstraint("item_count_" + std::to_string(i),
                                   indices,
                                   coefficients,
                                   'E',
                                   writer.getPartition()[i]);
    }
}

void createCommon12bVariables(InstanceWriter& writer,
                              CplexSolver& solver) {
    const bool usePiecewiseRule = (writer.getNumberOfPieces() > 1);
    const int numberOfStages = writer.getNumberOfStages();
    const int numberOfBars = writer.getNumberOfBars();
    const int numberOfItems = writer.getNumberOfItems();
    const int numberOfPieces = writer.getNumberOfPieces();

    for (int b = 0; b < numberOfBars; ++b) {
        solver.AddBinaryVariable(aiName(b));
    }

    for (int stage = 1; stage < numberOfStages; ++stage) {
        for (int b = 0; b < numberOfBars; ++b) {
            for (int i = 0; i < numberOfItems; ++i) {
                solver.AddBinaryVariable(aName(stage - 1, b, i));
            }
        }
    }

    for (int stage = 1; stage < numberOfStages; ++stage) {
        for (int i = 0; i < numberOfItems; ++i) {
            solver.AddBinaryVariable(zName(stage, i));
        }
    }

    for (int b = 0; b < numberOfBars; ++b) {
        solver.AddBinaryVariable(fName(b));
        solver.AddContinuousVariable(rName(b), 0.0, writer.getMinimumRest());
        solver.AddBinaryVariable(nrName(b));
    }

    for (int stage = 1; stage < numberOfStages; ++stage) {
        for (int b = 0; b < numberOfBars; ++b) {
            solver.AddBinaryVariable(wName(stage - 1, b));
        }
    }

    if (usePiecewiseRule) {
        for (int stage = 1; stage < numberOfStages; ++stage) {
            for (int b = 0; b < numberOfBars; ++b) {
                for (int i = 0; i < numberOfItems; ++i) {
                    solver.AddContinuousVariable(aTopName(stage - 1, b, i), -1000.0, 1000.0);
                    solver.AddContinuousVariable(aBottomName(stage - 1, b, i), -1000.0, 1000.0);

                    for (int p = 0; p < numberOfPieces; ++p) {
                        solver.AddBinaryVariable(zTopName(stage - 1, b, i, p));
                        solver.AddBinaryVariable(zBottomName(stage - 1, b, i, p));
                    }
                }
            }
        }
    }
}

void fixInitialBar(InstanceWriter& writer,
                   CplexSolver& solver,
                   const DR_Holder& decisionRule) {
    const int numberOfBars = writer.getNumberOfBars();

    for (int b = 0; b < numberOfBars; ++b) {
        std::vector<int> indices;
        std::vector<double> coefficients;

        addTerm(solver, indices, coefficients, aiName(b), 1.0);

        solver.AddLinearConstraint("fix_initial_bar_" + std::to_string(b),
                                   indices,
                                   coefficients,
                                   'E',
                                   (b == decisionRule.FirstStageBar) ? 1.0 : 0.0);
    }
}

void setTargetObjective(InstanceWriter& writer,
                        CplexSolver& solver,
                        const DecisionRuleSeparationTarget& target) {
    const int numberOfStages = writer.getNumberOfStages();
    const int numberOfItems = writer.getNumberOfItems();

    switch (target.type) {
    case DecisionRuleSeparationTarget::Type::UsedBarLink:
        /*
         * Master row:
         *   ai_b + sum a_{t,b,i} - T f_b <= 0
         */
        setObj(solver, aiName(target.bar), 1.0);
        for (int stage = 1; stage < numberOfStages; ++stage) {
            for (int i = 0; i < numberOfItems; ++i) {
                setObj(solver, aName(stage - 1, target.bar, i), 1.0);
            }
        }
        setObj(solver, fName(target.bar), -numberOfStages);
        break;

    case DecisionRuleSeparationTarget::Type::CapacityUpper:
        /*
         * Master row:
         *   used_length + r_b - bar_size_b <= 0
         */
        setObj(solver, aiName(target.bar), writer.getInitialLength());
        for (int stage = 1; stage < numberOfStages; ++stage) {
            for (int i = 0; i < numberOfItems; ++i) {
                setObj(solver, aName(stage - 1, target.bar, i), writer.getItemLength(i));
            }
        }
        setObj(solver, rName(target.bar), 1.0);
        addObjectiveConstant(solver, "capacity_" + std::to_string(target.bar), -writer.getBarSize(target.bar));
        break;

    case DecisionRuleSeparationTarget::Type::RestLower:
        /*
         * Master row:
         *   used_length + r_b + bar_size_b nr_b >= bar_size_b
         *
         * Violation:
         *   bar_size_b - used_length - r_b - bar_size_b nr_b
         */
        setObj(solver, aiName(target.bar), -writer.getInitialLength());
        for (int stage = 1; stage < numberOfStages; ++stage) {
            for (int i = 0; i < numberOfItems; ++i) {
                setObj(solver, aName(stage - 1, target.bar, i), -writer.getItemLength(i));
            }
        }
        setObj(solver, rName(target.bar), -1.0);
        setObj(solver, nrName(target.bar), -writer.getBarSize(target.bar));
        addObjectiveConstant(solver, "rest_lower_" + std::to_string(target.bar), writer.getBarSize(target.bar));
        break;

    case DecisionRuleSeparationTarget::Type::RestUpper:
        /*
         * Master row:
         *   used_length + minimum_rest nr_b - bar_size_b <= 0
         */
        setObj(solver, aiName(target.bar), writer.getInitialLength());
        for (int stage = 1; stage < numberOfStages; ++stage) {
            for (int i = 0; i < numberOfItems; ++i) {
                setObj(solver, aName(stage - 1, target.bar, i), writer.getItemLength(i));
            }
        }
        setObj(solver, nrName(target.bar), writer.getMinimumRest());
        addObjectiveConstant(solver, "capacity_" + std::to_string(target.bar), -writer.getBarSize(target.bar));
        break;

    case DecisionRuleSeparationTarget::Type::RestCap:
        /*
         * Master row:
         *   r_b + minimum_rest nr_b - minimum_rest <= 0
         */
        setObj(solver, rName(target.bar), 1.0);
        setObj(solver, nrName(target.bar), writer.getMinimumRest());
        addObjectiveConstant(solver, "rest_cap_" + std::to_string(target.bar), -writer.getMinimumRest());
        break;

    case DecisionRuleSeparationTarget::Type::AssignUpper:
        /*
         * Master row:
         *   sum_b a_{t,b,i} - z_{t,i} <= 0
         */
        for (int b = 0; b < writer.getNumberOfBars(); ++b) {
            setObj(solver, aName(target.stage - 1, b, target.item), 1.0);
        }
        setObj(solver, zName(target.stage, target.item), -1.0);
        break;

    case DecisionRuleSeparationTarget::Type::AssignLower:
        /*
         * Master row:
         *   z_{t,i} - sum_b a_{t,b,i} <= 0
         */
        setObj(solver, zName(target.stage, target.item), 1.0);
        for (int b = 0; b < writer.getNumberOfBars(); ++b) {
            setObj(solver, aName(target.stage - 1, b, target.item), -1.0);
        }
        break;

    case DecisionRuleSeparationTarget::Type::Switching:
        /*
         * Master row:
         *   sum_i a_{t,b,i} - previous_use_{t-1,b} - w_{t,b} <= 0
         */
        for (int i = 0; i < numberOfItems; ++i) {
            setObj(solver, aName(target.stage - 1, target.bar, i), 1.0);
        }

        if (target.stage == 1) {
            setObj(solver, aiName(target.bar), -1.0);
        } else {
            for (int i = 0; i < numberOfItems; ++i) {
                setObj(solver, aName(target.stage - 2, target.bar, i), -1.0);
            }
        }

        setObj(solver, wName(target.stage - 1, target.bar), -1.0);
        break;
    }
}

} // namespace

/*
 * Build the list of master constraints that have to be checked by the old 12b
 * separation model.
 *
 * Unlike the old implementation, this no longer returns LP-format strings.
 * It returns structured targets, so the model can be built internally.
 */
std::vector<DecisionRuleSeparationTarget>
InstanceWriter::PrepareDecisionRuleConstraintSeparationTargets(
    const DR_Holder& decisionRule)
{
    (void)decisionRule;

    std::vector<DecisionRuleSeparationTarget> targets;

    for (int b = 0; b < NumberOfBars; ++b) {
        targets.push_back(DecisionRuleSeparationTarget{
            DecisionRuleSeparationTarget::Type::UsedBarLink, 0, b, 0});
        targets.push_back(DecisionRuleSeparationTarget{
            DecisionRuleSeparationTarget::Type::CapacityUpper, 0, b, 0});
        targets.push_back(DecisionRuleSeparationTarget{
            DecisionRuleSeparationTarget::Type::RestLower, 0, b, 0});
        targets.push_back(DecisionRuleSeparationTarget{
            DecisionRuleSeparationTarget::Type::RestUpper, 0, b, 0});
        targets.push_back(DecisionRuleSeparationTarget{
            DecisionRuleSeparationTarget::Type::RestCap, 0, b, 0});
    }

    for (int stage = 1; stage < NumberOfStages; ++stage) {
        for (int i = 0; i < NumberOfItems; ++i) {
            targets.push_back(DecisionRuleSeparationTarget{
                DecisionRuleSeparationTarget::Type::AssignUpper, stage, 0, i});
            targets.push_back(DecisionRuleSeparationTarget{
                DecisionRuleSeparationTarget::Type::AssignLower, stage, 0, i});
        }

        for (int b = 0; b < NumberOfBars; ++b) {
            targets.push_back(DecisionRuleSeparationTarget{
                DecisionRuleSeparationTarget::Type::Switching, stage, b, 0});
        }
    }

    return targets;
}

/*
 * Build the old 12b separation problem directly in CPLEX.
 *
 * This model maximizes the violation of one structured master constraint
 * target. If the optimal value is positive, ExtractScenario(...) can recover
 * the item sequence from the selected z variables.
 */
void InstanceWriter::BuildDecisionRuleConstraintSeparationModel(
    CplexSolver& solver,
    const DR_Holder& decisionRule,
    const DecisionRuleSeparationTarget& target,
    int targetIndex)
{
    solver.CreateProblem(BaseName + "_DR12b_" + std::to_string(targetIndex));
    solver.SetMaximize();

    createCommon12bVariables(*this, solver);
    fixInitialBar(*this, solver, decisionRule);
    addFixedDecisionRuleConstraints(*this, solver, decisionRule);
    addScenarioCompositionConstraints(*this, solver);

    setTargetObjective(*this, solver, target);
}

/*
 * Compatibility guards.
 */
std::vector<std::string> InstanceWriter::prepareLines()
{
    throw std::runtime_error(
        "prepareLines() is deprecated. Use PrepareDecisionRuleConstraintSeparationTargets(...) instead.");
}

void InstanceWriter::writeDR_12b(DR_Holder, std::string, int)
{
    throw std::runtime_error(
        "writeDR_12b() is deprecated. Use BuildDecisionRuleConstraintSeparationModel(...) instead.");
}
