#ifndef VARIABLE_H_
#define VARIABLE_H_

#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

struct Variable{
public:
	int NumberOfIndices;
	std::vector< int > RangesOfIndices;
	std::vector<int> Offsets;
	std::vector<int> FixedIndices;
	std::vector< string >* ScenarioID;
	string VarName;
	double LB;
	double UB;
	bool free;
	bool ScenarioDependant;
	int ScenarioLocation;

	Variable(string name, int numInd, double lb, double ub, bool Scen=false){
		NumberOfIndices=numInd;
		VarName=name;
		for (int i=0;i<NumberOfIndices;i++){
			RangesOfIndices.push_back(0);	
			Offsets.push_back(0);	
			FixedIndices.push_back(-1);
		}
		ScenarioDependant=Scen;
		ScenarioLocation=-1;
		LB=lb;
		UB=ub;
	}

	void InitScenarios(vector<string> &ScenVec){
		ScenarioID= &ScenVec;
	}

	void AddIndex(int Location, int Range, int Offset=0, bool ScenIndex = false){
		RangesOfIndices[Location]=Range;
		Offsets[Location]=Offset;
		if(ScenIndex) ScenarioLocation=Location;
	}
	void FixIndex(int Location,int Value){
		FixedIndices[Location]=Value;
	}
	void FreeIndices(){
		for (int i=0;i<NumberOfIndices;i++){	
			FixedIndices[i]=-1;
		}
	}


	void Print(ofstream& out, bool WithBounds=false, bool IsFree=false){
		bool Finished=false;
		vector<int> Indices;
		for(int i=0;i<NumberOfIndices;i++)
			if(FixedIndices[i]>=0)
				Indices.push_back(FixedIndices[i]);
			else
				Indices.push_back(Offsets[i]);
		while(!Finished){
			if(WithBounds){
				if(!IsFree) out<<LB<<" <= ";
			}
			out<<VarName;
			for(int i=0;i<NumberOfIndices;i++)
				if(i!= ScenarioLocation)
					out <<"_"<<Indices[i];
				else out <<"_"<<ScenarioID->at(Indices[i]);
			if(WithBounds){
				if(!IsFree) out<<" <= "<<UB;
				else out <<" free";
			}
			out<<"\n";
			int ChangeAt=0;
			while(ChangeAt<NumberOfIndices && (Indices[ChangeAt]==RangesOfIndices[ChangeAt]-1 || FixedIndices[ChangeAt]>=0 )){
				if(FixedIndices[ChangeAt]<0){
					Indices[ChangeAt]=Offsets[ChangeAt];
				}
				ChangeAt++;
			}	
			if(ChangeAt<NumberOfIndices) {
				if(FixedIndices[ChangeAt]<0){
					Indices[ChangeAt]++;
				}
			}
			else  Finished=true;
		}
		FreeIndices();	
	}
};

#endif //VARIABLE_H_