#ifndef SCENARIO_TREE_H_
#define SCENARIO_TREE_H_
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include "ScenarioEncoding.h"
class Node{
public:
	int NumberOfSuccessors;
	std::vector < int > Successors;
	std::vector < int > RowIndices;
	int Parent;
	int Partner;
	bool IsNew;
	Node(int NumSuccessors, int L, bool New=true);
	~Node();
	std::string CurrentSubScenario;
	int level;
};
class Tree{
public:
	Tree(std::vector<int> SuccessorsInEachStage);
	std::vector<int> SuccessorsPerStage;
	int Depth;
	Node RootNode;
	std::vector < std::vector< std::string > > SubScenarios;
	std::vector < std::vector< int > > SubScenariosToNode;
	int BuildEntireTree(std::vector< int >  Partition);
	int ExpandTree( std::vector< std::string >  AdditionalScenarios );
	void CleanTree(std::vector< std::string > AdditionalScenarios = std::vector<std::string>());
	void FilterNewScenarios(std::vector<std::string>& Scenarios) const;
	bool ContainsScenario(const std::string& Scenario) const;
	void InitializePartialTree(std::vector< int >  Partition); 
	std::vector < std::string > ContainedScenarios;
	std::vector < std::string > AllAvailableScenarios;
	std::vector < Node > CurrentTree;
	std::mt19937 Rng;
	void random_stringShuffle(std::vector <std::string>& vec);
	int getRandomInteger(int LB,int UB);
	void setRandomSeed(int seed);

	//Tree(int SuccessorsPerLevel, int Depth);

};

#endif
