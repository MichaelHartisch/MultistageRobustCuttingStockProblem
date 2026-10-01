#include <iostream>
#include <stdexcept>
#include <random>
#include "InstanceWriter.h"

double InstanceWriter::getRandomDouble(double LB,double UB){
	if (UB < LB) {
		throw std::invalid_argument("Invalid random double bounds");
	}
	std::uniform_real_distribution<double> dist(LB, UB);
	return dist(Rng);
}

int InstanceWriter::getRandomInteger(int LB,int UB){
	if (UB < LB) {
		throw std::invalid_argument("Invalid random integer bounds");
	}
	std::uniform_int_distribution<int> dist(LB, UB);
	return dist(Rng);
}

void InstanceWriter::BuildAllUniqueParts(int m, int k) {
   	std::vector <int> Current (k,1);
   	Current[0]+=m-k;
   	Partitions.push_back(Current);
   	bool NewFound=true;
	if(Debug_Print){
		std::cerr <<"Possible Partitions:"<<std::endl;
		for (std::size_t n = 0; n < Current.size(); ++n)
   			std::cerr << Current[n] << " ";
   		std::cerr << std::endl;
   	}
   	while (NewFound){
	   	NewFound=false;
	   	for(int i=k-2;i>=0;i--){
	   		if(Current[i]-1>=Current[i+1]+1){
	   			Current[i]--;
	   			Current[i+1]++;
	   			Partitions.push_back(Current);
	   			NewFound=true;
	   			if(Debug_Print){
		   			for (std::size_t n = 0; n < Current.size(); ++n)
		   				std::cerr << Current[n] << " ";
	   				std::cerr << std::endl;
	   			}
	   			break;
	   		}
	   	}
   	}
   	if(Debug_Print)std::cerr <<std::endl;
}

/*void InstanceWriter::CreateScenarios(){
	std::string s = "";	
	SubScenarios.clear();
	for (int t=0;t<NumberOfStages-1;t++){
		std::vector<std::string > EmptySet;
		SubScenarios.push_back(EmptySet);
	}
	assert(NumberOfItems<=10);
	for (int i=0; i<NumberOfItems; i++){	
		for (int p=0;p< Partitions[RandPart][i];p++){
			s+=std::to_string(i);
		}
	}
    std::sort(s.begin(), s.end());
    do {
        //if(Debug_Print) std::cout << s << '\n';
        for(int t=0;t<NumberOfStages-1;t++){
        	if (std::find(SubScenarios[t].begin(), SubScenarios[t].end(),s.substr(0, t+1)) == SubScenarios[t].end()){
        		SubScenarios[t].push_back(s.substr(0, t+1));
        		//std::cerr << SubScenarios[t].back()<<std::endl;
        	}
        	if(t==NumberOfStages-2){
        		GlobalScenarios.push_back(s.substr(0, t+1));
        		GlobalScenarios_active.push_back(true);		
        	} 
        }
        //std::std::cerr<<s[0]<<std::endl;
    } while(std::next_permutation(s.begin(), s.end()));
    if(Debug_Print){
    	for(int t=0;t<NumberOfStages-1;t++){
        	std::cerr <<"There are " <<SubScenarios[t].size() << " SubScenarios of length "<<(t+1) <<std::endl;
        }
    } 
}*/

void InstanceWriter::random_shuffle(std::vector <bool> & vec){
    for (int i = vec.size()-1; i > 0; --i) {
        using std::swap;
        swap(vec[i], vec[getRandomInteger(0,vec.size()-1)]);
    }
}

void InstanceWriter::InitSelectedScenarios(int HowMany){
	std::vector<bool> esv(GlobalScenarios.size(),0);

	std::fill_n(esv.begin(),HowMany,1);
	random_shuffle(esv);
	std::swap(GlobalScenarios_active,esv);
}


/*void InstanceWriter::OnlySelectedScenarios(){
	std::string s = "";	
	SubScenarios.clear();
	for (int t=0;t<NumberOfStages-1;t++){
		std::vector<std::string > EmptySet;
		SubScenarios.push_back(EmptySet);
	}
	assert(NumberOfItems<=10);
	for (std::size_t i = 0; i < GlobalScenarios.size(); ++i){
		//std::cerr<<"i="<<i<<std::endl;
		if(GlobalScenarios_active[i]){
			s=GlobalScenarios[i];
			for(int t=0;t<NumberOfStages-1;t++){
	        	if (std::find(SubScenarios[t].begin(), SubScenarios[t].end(),s.substr(0, t+1)) == SubScenarios[t].end()){
	        		SubScenarios[t].push_back(s.substr(0, t+1));
	        		//std::cerr << SubScenarios[t].back()<<std::endl;
	        	}
	        }
		}	

	}
    if(Debug_Print){
    	for(int t=0;t<NumberOfStages-1;t++){
        	std::cerr <<"There are " <<SubScenarios[t].size() << " SubScenarios of length "<<(t+1) <<std::endl;
        }
    } 
}*/