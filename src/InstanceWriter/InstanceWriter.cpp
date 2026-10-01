#include "InstanceWriter.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include "ScenarioEncoding.h"
#include <filesystem>
#include <stdexcept>

using namespace std;


namespace {
std::string trim(const std::string& value) {
    std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string stemFromPath(const std::string& path) {
    std::size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const std::vector<std::string> suffixes = {".info", ".raw"};
    for (const std::string& suffix : suffixes) {
        if (name.size() >= suffix.size() && name.substr(name.size() - suffix.size()) == suffix) {
            name.resize(name.size() - suffix.size());
            break;
        }
    }
    return name;
}
}

InstanceWriter::InstanceWriter(int Stages, int Bars, int Items, int Rand,string Base, int NumP, int ConsStages, std::vector<int> SuccessorsInEachStage, CplexSolver* TheSolver): RandomSeed(Rand), Rng(static_cast<std::mt19937::result_type>(Rand)), Solver(TheSolver), STree(SuccessorsInEachStage), FullTree(SuccessorsInEachStage)
{ 
	NumberOfStages=Stages;
	NumberOfBars=Bars;
	NumberOfItems=Items;
	ScenarioEncoding::validateNumberOfItems(NumberOfItems);
	STree.setRandomSeed(Rand);
	FullTree.setRandomSeed(Rand);
	BaseName=Base;
	P=NumP;
	ConsideredStages=ConsStages;
}

std::vector<int> InstanceWriter::UniformSuccessors(int Stages, int Items) {
    std::vector<int> successors;
    for (int t = 0; t < Stages - 1; ++t) {
        successors.push_back(Items);
    }
    successors.push_back(0);
    return successors;
}

InstanceWriter InstanceWriter::FromInfoFile(const std::string& InfoFile, const std::string& OutputBaseName, int NumP, int ConsStages) {
    std::ifstream in(InfoFile.c_str());
    if (!in) {
        throw std::runtime_error("Could not open info file: " + InfoFile);
    }

    int stages = -1;
    int bars = -1;
    int items = -1;
    std::vector<double> barSizes;
    std::vector<double> itemLengths;
    std::vector<int> itemQuantities;
    int initialItem = -1;

    std::string line;
    bool readItems = false;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind("T=", 0) == 0) {
            stages = std::stoi(trim(line.substr(2)));
            readItems = false;
        } else if (line.rfind("B=", 0) == 0) {
            bars = std::stoi(trim(line.substr(2)));
            readItems = false;
        } else if (line.rfind("I=", 0) == 0) {
            items = std::stoi(trim(line.substr(2)));
            readItems = false;
        } else if (line.rfind("Length:", 0) == 0) {
            readItems = false;
            std::string values = line.substr(std::string("Length:").size());
            std::stringstream ss(values);
            std::string cell;
            while (std::getline(ss, cell, ';')) {
                cell = trim(cell);
                if (!cell.empty()) barSizes.push_back(std::stod(cell));
            }
        } else if (line.rfind("ItemNumber", 0) == 0) {
            readItems = true;
        } else if (line.rfind("Initial Item=", 0) == 0) {
            readItems = false;
            initialItem = std::stoi(trim(line.substr(std::string("Initial Item=").size())));
        } else if (line.rfind("Number Of Scenarios=", 0) == 0) {
            readItems = false;
        } else if (readItems) {
            std::stringstream ss(line);
            std::string idxStr, lengthStr, quantityStr;
            if (std::getline(ss, idxStr, ';') && std::getline(ss, lengthStr, ';') && std::getline(ss, quantityStr, ';')) {
                int idx = std::stoi(trim(idxStr));
                if (idx < 0) throw std::runtime_error("Negative item index in info file: " + InfoFile);
                if (static_cast<std::size_t>(idx) >= itemLengths.size()) {
                    itemLengths.resize(static_cast<std::size_t>(idx) + 1);
                    itemQuantities.resize(static_cast<std::size_t>(idx) + 1);
                }
                itemLengths[static_cast<std::size_t>(idx)] = std::stod(trim(lengthStr));
                itemQuantities[static_cast<std::size_t>(idx)] = std::stoi(trim(quantityStr));
            }
        }
    }

    if (stages <= 0 || bars <= 0 || items <= 0) {
        throw std::runtime_error("Info file is missing a valid T, B, or I value: " + InfoFile);
    }
    ScenarioEncoding::validateNumberOfItems(items);
    if (static_cast<int>(barSizes.size()) != bars) {
        throw std::runtime_error("Info file bar-length count does not match B: " + InfoFile);
    }
    if (static_cast<int>(itemLengths.size()) != items || static_cast<int>(itemQuantities.size()) != items) {
        throw std::runtime_error("Info file item table does not match I: " + InfoFile);
    }
    if (initialItem < 0 || initialItem >= items) {
        throw std::runtime_error("Info file is missing a valid initial item: " + InfoFile);
    }
    if (itemQuantities[static_cast<std::size_t>(initialItem)] <= 0) {
        throw std::runtime_error("Initial item quantity must be at least one in info file: " + InfoFile);
    }

    if (ConsStages < 0) {
        ConsStages = stages;
    }

    std::string base = OutputBaseName.empty() ? stemFromPath(InfoFile) : OutputBaseName;
    InstanceWriter instance(stages, bars, items, 0, base, NumP, ConsStages, UniformSuccessors(stages, items));

    instance.Debug_Print = false;
    instance.CostPerBar = 4;
    instance.CostPerUnit = 1;
    instance.MaxBarSize = 100;
    instance.MinBarSize = 30;
    instance.LengthCostFactor = .5;
    instance.MinimumRest = 10;
    instance.BarChangeCost = instance.MinimumRest * instance.CostPerUnit / 2;

    instance.BarSizes = barSizes;
    instance.LengthOfItems = itemLengths;
    instance.IndexInitial = initialItem;
    instance.LengthOfInitial = itemLengths[static_cast<std::size_t>(initialItem)];

    std::vector<int> partition = itemQuantities;
    partition[static_cast<std::size_t>(initialItem)]--;
    instance.Partitions.clear();
    instance.Partitions.push_back(partition);
    instance.RandPart = 0;
    instance.RandomSeed = 0;
    instance.Rng.seed(0);
    instance.STree.setRandomSeed(0);
    instance.FullTree.setRandomSeed(0);
    instance.GlobalScenarios.clear();
    instance.GlobalScenarios_active.clear();

    return instance;
}


void InstanceWriter::Initialize(){
	Debug_Print=true;
	if (Debug_Print){
		//cout <<"Number of Stages: " <<NumberOfStages<< "\n";
		//cout <<"Number of Bars: " <<NumberOfBars << "\n";
		//cout <<"Number of Items: " <<NumberOfItems << "\n";
	}
	if (NumberOfStages<NumberOfItems){
		//cout <<"Too many items for too few stages" << endl;
		exit(1);
	}

	CostPerBar=4;
	CostPerUnit=1;
	MaxBarSize=100;
	MinBarSize=30;
	LengthCostFactor=.5;
	MinimumRest= 10;//RealMaxBarSize/6;
	BarChangeCost=MinimumRest*CostPerUnit/2/*/2*/; //Nicht mehr nochmal durch 2, da nicht mehr doppelt in ZF bestraft

	BuildAllUniqueParts(NumberOfStages,NumberOfItems);
	RandPart = getRandomInteger(0, static_cast<int>(Partitions.size()) - 1);

	double RealMinBarSize=MaxBarSize;
	double RealMaxBarSize=MinBarSize;
	double AverageBarSize=0;
	for(int b=0;b<NumberOfBars;b++){
		BarSizes.push_back(getRandomInteger(MinBarSize,MaxBarSize));
		//if (Debug_Print) cout << "BarSize " << b << " : "<< BarSizes.back()<<endl;
		if(BarSizes.back()<RealMinBarSize) RealMinBarSize=BarSizes.back();
		if(BarSizes.back()>RealMaxBarSize) RealMaxBarSize=BarSizes.back();
		AverageBarSize+=BarSizes.back();
	}
	//if (Debug_Print) cout << "MinRest: "<< MinimumRest<<endl;
	AverageBarSize/=NumberOfBars;
	for(int i=0;i<NumberOfItems;i++){
		LengthOfItems.push_back(getRandomInteger(MinimumRest,(int)AverageBarSize*NumberOfBars/NumberOfStages));
		//if (Debug_Print) cout << "ItemLength " << i << " : "<< LengthOfItems.back()<<endl;
	}
	std::vector<int> eligibleInitialItems;
	for (int i = 0; i < NumberOfItems; ++i) {
		if (Partitions[RandPart][i] > 0) {
			eligibleInitialItems.push_back(i);
		}
	}
	if (eligibleInitialItems.empty()) {
		throw std::runtime_error("Selected partition contains no item that can be used as initial item");
	}
	IndexInitial = eligibleInitialItems[static_cast<std::size_t>(getRandomInteger(0, static_cast<int>(eligibleInitialItems.size()) - 1))];
    LengthOfInitial=LengthOfItems[IndexInitial];
    	//if (Debug_Print) cout << "LengthOfInitial: "<< LengthOfInitial<<endl;
	Partitions[RandPart][IndexInitial]--;
	if (Debug_Print){
		//cout << "BarChangeCost: " <<BarChangeCost  << "\n";
		//cout << "LengthCostFactor: " <<LengthCostFactor  << "\n";
		//cout << "RealMinBarSize: " <<RealMinBarSize << "\n";
		//cout << "RealMaxBarSize: " <<RealMaxBarSize << "\n";
	}
}

std::vector<int> InstanceWriter::getPartition(){
	return Partitions[RandPart];
}

void InstanceWriter::CreateDataSheet()
{
    const std::filesystem::path outputPath = std::filesystem::path(BaseName + ".raw");

    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream out(outputPath);

    if (!out.is_open()) {
        throw std::runtime_error(
            "Could not open instance output file for writing: " + outputPath.string()
        );
    }
	//stringstream mipout;
	//mipout<<BaseName<<".raw";
	//ofstream out(mipout.str().c_str());
	out << "T="<<NumberOfStages<<endl;
	out << "B="<<NumberOfBars<<endl;
	out << "I="<<NumberOfItems<<endl;
	out << "Seed="<<RandomSeed<<endl<<endl;
	out << "Length:";
	for(int b=0;b<NumberOfBars;b++){
		out << BarSizes[b];
		if(b<NumberOfBars-1) out << ";";
	}
	out <<endl<<endl;
	out << "ItemNumber;ItemLenght;ItemQuantity:" << endl;
	for(int i=0;i<NumberOfItems;i++){
		out << i<< ";"<<LengthOfItems[i]<<";";
		if(i==IndexInitial) out<< Partitions[RandPart][i]+1<<endl;
		else out<< Partitions[RandPart][i]<<endl;
	}
	out <<endl;
	out << "Initial Item="<<IndexInitial << endl <<endl;
	out.close();


}
