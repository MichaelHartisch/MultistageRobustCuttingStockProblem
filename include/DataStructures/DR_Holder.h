#ifndef DR_HOLDER_H
#define DR_HOLDER_H

#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include "ScenarioEncoding.h"
using namespace std;

struct DR_Holder{
public:
	double opt;
	int FirstStageBar;
	int Stages;
	vector<vector<vector<vector<vector<double > > > > > yTop;
	vector<vector<vector<vector<vector<double > > > > > yBot;
	vector<vector<vector<vector<double > > > > yTopConst;
	vector<vector<vector<vector<double > > > > yBotConst;
	std::vector< std::string >  NecessaryScenarios;
	std::vector<int> ScenarioCollector;
	static constexpr double BinaryTolerance = 1e-6;

	void 	PrepareForScenarioCollection(){
		for (int i=0;i<Stages;i++)
			ScenarioCollector[i]=-1;
	}

	void CollectScenario(string VarName, double Value){
		if(std::fabs(Value - 1.0) <= BinaryTolerance){
			std::string delimiter = "_";
			size_t pos = 0;
			std::string token;

			pos = VarName.find(delimiter);
		    token = VarName.substr(0, pos);
		    if (token[0]!='z' && token.length()>1){ 
		    	std::cerr<<"Wrong variable for Scenario Collection! " << token <<endl;
		    	exit(0);
		    }
		    VarName.erase(0, pos + delimiter.length());

		    pos = VarName.find(delimiter);
		    token = VarName.substr(0, pos);
		    int Stage=atoi(token.c_str());
		    VarName.erase(0, pos + delimiter.length());

		    int Item=atoi(VarName.c_str());

		    ScenarioCollector[Stage-1]=Item;
		}
	}

	bool BuildAndAddScenarioString(){
		std::string Scen="";
		for (int i=0;i<Stages;i++){
			if(ScenarioCollector[i]==-1){
				std::cerr <<"ERROR: Did not collect a complete scenario" << std::endl;
				return false;
			}
			Scen += ScenarioEncoding::encodeItem(ScenarioCollector[i]);

		}
		//std::cerr << "Found Scenario is " << Scen << std::endl;
		if (std::find(NecessaryScenarios.begin(), NecessaryScenarios.end(), Scen) == NecessaryScenarios.end()) {
			NecessaryScenarios.push_back(Scen);
		}
		return true;
	}

	DR_Holder(int NumberOfStages, int NumberOfBars, int NumberOfItems, int P){
		Stages=NumberOfStages-1;
		std::cerr << "Stages in DR_Holder is " << Stages<<std::endl;
		ScenarioCollector.clear();
		for (int i=0;i<Stages;i++)
			ScenarioCollector.push_back(-1);
		for (int t=0;t<NumberOfStages-1;t++){
			vector<vector<vector<vector<double > > > > Ts;
			for (int b=0;b<NumberOfBars;b++){
				vector<vector<vector<double > > > Bs;
				for (int i=0;i<NumberOfItems;i++){
					vector<vector<double > > Is;
					for (int p=0;p<P;p++){
						vector<double > Ps;
						for(int w=0;w<NumberOfItems*(t+1);w++){
							Ps.push_back(0);
						}
						Is.push_back(Ps);
					}
					Bs.push_back(Is);
				}
				Ts.push_back(Bs);
			}
			yTop.push_back(Ts);
			yBot.push_back(Ts);
		}

		for (int t=0;t<NumberOfStages-1;t++){
			vector<vector<vector<double > > > Ts;
			for (int b=0;b<NumberOfBars;b++){
				vector<vector<double > > Bs;
				for (int i=0;i<NumberOfItems;i++){
					vector<double > Is;
					for (int p=0;p<P;p++){
						Is.push_back(0);
					}
					Bs.push_back(Is);
				}
				Ts.push_back(Bs);
			}
			yTopConst.push_back(Ts);
			yBotConst.push_back(Ts);
		}
	}

	void SetFirstStageBar(string BarVar){
		//std::cout << BarVar << " representens first stage solution" <<endl;
		std::string delimiter = "_";
		size_t pos = BarVar.find(delimiter);
	    BarVar.erase(0, pos + delimiter.length());
	    FirstStageBar=atoi(BarVar.c_str());
	}

	void SetOptimalValue(double in_opt){
		opt=in_opt;
	}

	void setConst(string VarName, double Value){
		std::cout.precision(10);
		//std::cout << "Set Const " <<VarName << "=" << Value << endl;
		std::string s = VarName;
		std::string delimiter = "_";
		size_t pos = 0;
		std::string token;
		bool yT=false;

		//while ((pos = s.find(delimiter)) != std::string::npos) {
		pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    if (token[1]=='T') yT=true;
	    if (s[2]=='_') std::cout << "Warning in setConst()" << std::endl;
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int t=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int b=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int i=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int p=atoi(token.c_str());
	    /*if (test*100000<1){ 
	    	cerr <<"RoundedA " << Value << " to " << floor(Value+.5)<<endl;
	    	Value =floor(Value+.5);
	    }
	    if(fabs(Value-floor(Value+.5))<0.00001){ 
	    	cerr <<"Rounded " << Value << " to " << floor(Value+.5)<<endl;
	    	Value =floor(Value+.5);
	    }*/
	    if(yT) yTopConst[t][b][i][p]=Value;
	    else yBotConst[t][b][i][p]=Value;

		//}
	}

	void set(string VarName, double Value){
		std::cout.precision(10);
		//std::cout << VarName << "=" << Value << endl;
		std::string s = VarName;
		std::string delimiter = "_";
		size_t pos = 0;
		std::string token;
		bool yT=false;

		//while ((pos = s.find(delimiter)) != std::string::npos) {
		pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    if (token[1]=='T') yT=true;
	    if (s[2]!='_') std::cout << "Warning in set()" << std::endl;
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int t=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int b=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int i=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());

	    pos = s.find(delimiter);
	    token = s.substr(0, pos);
	    int p=atoi(token.c_str());
	    s.erase(0, pos + delimiter.length());
	    int w=atoi(s.c_str());
	    /*if (test*100000<1){ 
	    	cerr <<"RoundedA " << Value << " to " << floor(Value+.5)<<endl;
	    	Value =floor(Value+.5);
	    }
	    if(fabs(Value-floor(Value+.5))<0.00001){ 
	    	cerr <<"Rounded " << Value << " to " << floor(Value+.5)<<endl;
	    	Value =floor(Value+.5);
	    }*/
	    if(yT) yTop[t][b][i][p][w]=Value;
	    else yBot[t][b][i][p][w]=Value;

		//}
	}
};



#endif //DR_HOLDER_H
