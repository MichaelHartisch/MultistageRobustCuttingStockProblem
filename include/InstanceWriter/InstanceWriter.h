#ifndef INSTANCE_WRITER_H_
#define INSTANCE_WRITER_H_

#include <cstddef>
#include <random>
#include <string>
#include <vector>

#include "DR_Holder.h"
#include "ScenarioTree.h"

class CplexSolver;

struct DecisionRuleSeparationTarget {
    enum class Type {
        UsedBarLink,
        CapacityUpper,
        RestLower,
        RestUpper,
        RestCap,
        AssignUpper,
        AssignLower,
        Switching
    };

    Type type;
    int stage;
    int bar;
    int item;
};

class InstanceWriter{
private:
    double getRandomDouble(double LB,double UB);
    int getRandomInteger(int LB,int UB);
    void BuildAllUniqueParts(int m, int k);
    int NumberOfStages;
    int NumberOfItems;
    int NumberOfBars;   // NumberOfBins
    int ConsideredStages;
    int P;
    bool Debug_Print;
    std::string BaseName;
    double MinimumRest;
    double BarChangeCost;
    double MaxBarSize;
    double MinBarSize;
    double LengthCostFactor;
    std::vector<double> BarSizes;
    std::vector< std::vector< int > > Partitions;
    int IndexInitial;
    double LengthOfInitial;
    double CostPerUnit;
    double CostPerBar;
    int RandPart;
    int RandomSeed;
    std::mt19937 Rng;
    std::vector < std::string > GlobalScenarios;
    std::vector < bool > GlobalScenarios_active;
    std::vector<double> LengthOfItems;
    CplexSolver* Solver;
public:
    Tree STree;
    Tree FullTree;

    InstanceWriter(int Stages, int Bars, int Items, int Rand, std::string BaseName, int NumP, int ConsStages, std::vector<int> SuccessorsInEachStage, CplexSolver* TheSolver = nullptr);
    static InstanceWriter FromInfoFile(const std::string& InfoFile, const std::string& OutputBaseName = "", int NumP = 1, int ConsStages = -1);
    static std::vector<int> UniformSuccessors(int Stages, int Items);
    void Initialize();
    void OnlySelectedScenarios();
    void random_shuffle(std::vector <bool>& vec);
    void InitSelectedScenarios(int HowMany);

    // Non-CPLEX model writers and instance generation.
    void writeQIP();
    void writeRO();
    void writeRO_RC();
    void writeRO_RC_alt();
    void writeMIP();
    void writeMIPasQLP();
    void writeHeuristic2();
    void writeRC();
    void CreateScenarios();
    void CreateDataSheet();

    // Legacy/debug DR writers.  The internal decision-rule workflow should not
    // call these file-writing methods anymore.
    void writeDR(double Bound = 0 );
    void writeDR2();
    void writeDR_basic(double Bound = 0 );
    void DR_prepare();
    void writeDR_12a(DR_Holder DR);
    std::vector < std::string > prepareLines();
    void writeDR_12b(DR_Holder DR,std::string Line, int LineNr);

    // Internal CPLEX builders used by RunDecisionRule.
    void BuildInitialDecisionRuleMaster(CplexSolver& solver, double Bound = 0.0);
    void AddDecisionRuleMasterRowsForNewScenarios(CplexSolver& solver, const DR_Holder& decisionRule);
    void BuildDecisionRuleObjectiveSeparationModel(CplexSolver& solver, const DR_Holder& decisionRule);
    std::vector<DecisionRuleSeparationTarget> PrepareDecisionRuleConstraintSeparationTargets(const DR_Holder& decisionRule);
    void BuildDecisionRuleConstraintSeparationModel(CplexSolver& solver,
                                                    const DR_Holder& decisionRule,
                                                    const DecisionRuleSeparationTarget& target,
                                                    int targetIndex);

    void setCplexSolver(CplexSolver* TheSolver) { Solver = TheSolver; }

    std::vector<int> getPartition();
    int getNumberOfStages() const { return NumberOfStages; }
    int getNumberOfBars() const { return NumberOfBars; }
    int getNumberOfItems() const { return NumberOfItems; }
    int getNumberOfPieces() const { return P; }
    int getConsideredStages() const { return ConsideredStages; }
    double getMinimumRest() const { return MinimumRest; }
    double getInitialLength() const { return LengthOfInitial; }
    double getItemLength(int i) const { return LengthOfItems.at(static_cast<std::size_t>(i)); }
    double getBarSize(int b) const { return BarSizes.at(static_cast<std::size_t>(b)); }
    const std::string& getBaseName() const { return BaseName; }
};

#endif
