#include "ScenarioTree.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <random>
#include <algorithm>

Tree::Tree(std::vector<int> SuccessorsInEachStage): RootNode(SuccessorsInEachStage[0],0), Rng(0)
{
	SuccessorsPerStage.clear();
	for (std::size_t i = 0; i < SuccessorsInEachStage.size(); ++i)	
		SuccessorsPerStage.push_back(SuccessorsInEachStage[i]);
	this->Depth=SuccessorsPerStage.size()-1;
	std::cerr <<"Depth of Tree is " << this->Depth << std::endl;
}
int Tree::BuildEntireTree(	std::vector< int >  Partition){
	CurrentTree.clear();
	ContainedScenarios.clear();
	CurrentTree.push_back(RootNode);
	std::string s = "";	
	SubScenarios.clear();
	SubScenariosToNode.clear();
	for (int t=0;t<Depth;t++){
		std::vector<std::string > EmptySet;
		SubScenarios.push_back(EmptySet);
		std::vector<int> EmptyNodeSet;
		SubScenariosToNode.push_back(EmptyNodeSet);
	}
	for (std::size_t i = 0; i < Partition.size(); ++i){	
		for (int p=0;p< Partition[i];p++){
			s += ScenarioEncoding::encodeItem(static_cast<int>(i));
		}
	}
    std::sort(s.begin(), s.end());
    do {
    	int CurrentNode = 0; //RootNode
    	for(int t=0;t<Depth;t++){
    		//std::cerr <<"Current Successor is "<<ScenarioEncoding::decodeItem(s[t])<<std::endl;
    		if(CurrentTree[CurrentNode].NumberOfSuccessors==0) std::cerr << "Warning: Not enough successors in node " << std::endl;
	    	if (CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]==-1){
	    		Node NewNode(SuccessorsPerStage[t+1],t+1,false);
	    		NewNode.Parent=CurrentNode;
	    		NewNode.CurrentSubScenario = s.substr(0, t+1);
	    		CurrentTree.push_back(NewNode);
	    		CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]=CurrentTree.size()-1;
	    		SubScenarios[t].push_back(s.substr(0, t+1));
	    		SubScenariosToNode[t].push_back(static_cast<int>(CurrentTree.size())-1);
	    		
	    	}
	    	CurrentNode = CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])];
	    	if(t==Depth-1){
        		ContainedScenarios.push_back(s.substr(0, t+1));
        	} 
	    }
    } while(std::next_permutation(s.begin(), s.end()));
	for(int t=0;t<Depth;t++){
    	std::cerr <<"There are " <<SubScenarios[t].size() << " SubScenarios of length "<<(t+1) <<std::endl;
    }
    std::cerr <<"Number of contained Scenarios is " << ContainedScenarios.size()<<std::endl;
    return ContainedScenarios.size();
}

int Tree::getRandomInteger(int LB,int UB){
	if (UB < LB) {
		throw std::invalid_argument("Invalid random integer bounds");
	}
	std::uniform_int_distribution<int> dist(LB, UB);
	return dist(Rng);
}

void Tree::setRandomSeed(int seed){
	Rng.seed(static_cast<std::mt19937::result_type>(seed));
}

void Tree::random_stringShuffle(std::vector <std::string>& vec){
    for (int i = vec.size()-1; i > 0; --i) {
        using std::swap;
        swap(vec[i], vec[getRandomInteger(0,vec.size()-1)]);
    }
}

void Tree::InitializePartialTree(std::vector< int >  Partition){
	std::cerr <<"Start InitializePartialTree" << std::endl;
	CurrentTree.clear();
	CurrentTree.push_back(RootNode);

	std::string s = "";	
	SubScenarios.clear();
	SubScenariosToNode.clear();
	for (int t=0;t<Depth;t++){
		std::vector<std::string > EmptySet;
		SubScenarios.push_back(EmptySet);
		std::vector<int> EmptyNodeSet;
		SubScenariosToNode.push_back(EmptyNodeSet);
	}
	for (std::size_t i = 0; i < Partition.size(); ++i){	
		for (int p=0;p< Partition[i];p++){
			s += ScenarioEncoding::encodeItem(static_cast<int>(i));
		}
	}
	std::sort(s.begin(), s.end());
	AllAvailableScenarios.clear();
	ContainedScenarios.clear();
    do {
    	AllAvailableScenarios.push_back(s);
    } while(std::next_permutation(s.begin(), s.end()));
    	random_stringShuffle(AllAvailableScenarios);

    std::vector<std::string>::const_iterator first = AllAvailableScenarios.begin();
	std::vector<std::string>::const_iterator last = AllAvailableScenarios.begin() + (int)fmax(1,std::floor(std::log2(AllAvailableScenarios.size())));
	ContainedScenarios.assign(first,last);
   	for (std::size_t i = 0; i < ContainedScenarios.size(); ++i){	
   		std::string s = ContainedScenarios[i];
   		int CurrentNode = 0; //RootNode
    	for(int t=0;t<Depth;t++){
    		if(CurrentTree[CurrentNode].NumberOfSuccessors==0) std::cerr << "Warning: Not enough successors in node " << std::endl;
	    	if (CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]==-1){
	    		Node NewNode(SuccessorsPerStage[t+1],t+1);
	    		NewNode.Parent=CurrentNode;
	    		NewNode.CurrentSubScenario = s.substr(0, t+1);
	    		CurrentTree.push_back(NewNode);
	    		CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]=CurrentTree.size()-1;
	    		SubScenarios[t].push_back(s.substr(0, t+1));
	    		SubScenariosToNode[t].push_back(static_cast<int>(CurrentTree.size())-1);
	    		
	    	}
	    	CurrentNode = CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])];
	    }
    } 
	for(int t=0;t<Depth;t++){
    	std::cerr <<"There are " <<SubScenarios[t].size() << " SubScenarios of length "<<(t+1) <<std::endl;
    }
    std::cerr <<"Number of contained Scenarios is " << ContainedScenarios.size()<<std::endl;
}

int Tree::ExpandTree( std::vector< std::string >  AdditionalScenarios ){
	int Added=0;
	for (std::size_t i = 0; i < AdditionalScenarios.size(); ++i){
	    std::string s = AdditionalScenarios[i];
	    int CurrentNode = 0; //RootNode
    	for(int t=0;t<Depth;t++){
    		if(CurrentTree[CurrentNode].NumberOfSuccessors==0) std::cerr << "Warning: Not enough successors in node " << std::endl;
	    	if (CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]==-1){
	    		Node NewNode(SuccessorsPerStage[t+1],t+1);
	    		NewNode.Parent=CurrentNode;
	    		NewNode.CurrentSubScenario = s.substr(0, t+1);
	    		CurrentTree.push_back(NewNode);
	    		CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])]=CurrentTree.size()-1;
	    		SubScenarios[t].push_back(s.substr(0, t+1));
	    		SubScenariosToNode[t].push_back(static_cast<int>(CurrentTree.size())-1);
	    		if(t==Depth-1){
        			ContainedScenarios.push_back(s.substr(0, t+1));
        			Added++;
        		} 
	    	}
	    	CurrentNode = CurrentTree[CurrentNode].Successors[ScenarioEncoding::decodeItem(s[t])];
	    	
	    }
    } 
    if(AdditionalScenarios.size()>0){
    	std::cerr <<"Expanded Scenario Tree: Added " << Added << " of " << AdditionalScenarios.size() <<" scenarios"<<std::endl;
		for(int t=0;t<Depth;t++){
	    	std::cerr <<"There are " <<SubScenarios[t].size() << " SubScenarios of length "<<(t+1) <<std::endl;
	    }
	    std::cerr <<"Number of contained Scenarios is " << ContainedScenarios.size()<<std::endl;
	}
    return Added;
}

void Tree::CleanTree(std::vector< std::string > /*AdditionalScenarios*/){
    for (Node& node : CurrentTree) {
        node.IsNew = false;
    }
}

bool Tree::ContainsScenario(const std::string& Scenario) const {
    return std::find(ContainedScenarios.begin(), ContainedScenarios.end(), Scenario) != ContainedScenarios.end();
}

void Tree::FilterNewScenarios(std::vector<std::string>& Scenarios) const {
    std::vector<std::string> filtered;
    for (const std::string& scenario : Scenarios) {
        if (static_cast<int>(scenario.size()) != Depth) {
            std::cerr << "Ignoring malformed scenario of length " << scenario.size()
                      << "; expected " << Depth << ": " << scenario << std::endl;
            continue;
        }
        bool valid = true;
        for (char symbol : scenario) {
            try {
                ScenarioEncoding::decodeItem(symbol);
            } catch (const std::exception&) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            std::cerr << "Ignoring scenario with invalid symbols: " << scenario << std::endl;
            continue;
        }
        if (ContainsScenario(scenario)) {
            continue;
        }
        if (std::find(filtered.begin(), filtered.end(), scenario) == filtered.end()) {
            filtered.push_back(scenario);
        }
    }
    Scenarios.swap(filtered);
}
