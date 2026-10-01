#ifndef INSTANCE_WRITER_H_
#define INSTANCE_WRITER_H_

#include <string>
#include <vector>
#include "ScenarioTree.h"
#include "DR_Holder.h"
#include <random>
#include <cstddef>

class CplexSolver;

//#include <iostream>
class InstanceWriter{
private:
	double getRandomDouble(double LB,double UB);
	int getRandomInteger(int LB,int UB);
	void BuildAllUniqueParts(int m, int k);
	int NumberOfStages;
	int NumberOfItems;
	int NumberOfBars;	//NumberOfBins
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
	//std::vector < std::vector< std::string > > SubScenarios;
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
	void writeDR_12a(DR_Holder DR);
	void writeQIP();
	void writeRO();
	void writeRO_RC();
	void writeRO_RC_alt();
	void writeMIP();
	void writeMIPasQLP();
	void writeDR(double Bound = 0 );
	void writeDR_basic(double Bound = 0 );
	void DR_prepare();
	void setCplexSolver(CplexSolver* TheSolver) { Solver = TheSolver; }
	void writeDR2();
	void writeHeuristic2();
	void writeRC();
	void CreateScenarios();
	void CreateDataSheet();
std::vector < std::string > prepareLines();
	void writeDR_12b(DR_Holder DR,std::string Line, int LineNr);
	std::vector<int> getPartition();
	int getNumberOfStages() const { return NumberOfStages; }
	int getNumberOfBars() const { return NumberOfBars; }
	int getNumberOfItems() const { return NumberOfItems; }
	const std::string& getBaseName() const { return BaseName; }


};

#endif
