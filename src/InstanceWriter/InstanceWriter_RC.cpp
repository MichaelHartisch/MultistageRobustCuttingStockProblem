#include "InstanceWriter.h"
#include "Variable.h"
#include <iostream>
using namespace std;

void InstanceWriter::writeRC(){
	STree.BuildEntireTree(getPartition());
	Variable AI("ai",  1,  0,  1);
	AI.AddIndex(0,NumberOfBars);

	vector <Variable> A;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("a",  3,  0,  1,true);
		Av.InitScenarios(STree.SubScenarios[t]);
		Av.AddIndex(0,STree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		A.push_back(Av);
	}
	
	Variable F("f",  2,  0,  1,true);
	F.InitScenarios(STree.SubScenarios.back());
	F.AddIndex(0,STree.SubScenarios.back().size(),0,true);
    F.AddIndex(1,NumberOfBars,0);

    vector <Variable> W;
	for (int t=0;t<NumberOfStages;t++){
		Variable Wv("w",  2,  0,  1,true);
		Wv.InitScenarios(STree.SubScenarios[t]);
		Wv.AddIndex(0,STree.SubScenarios[t].size(),0,true);
		Wv.AddIndex(1,NumberOfBars);
		W.push_back(Wv);
	}

	Variable R("r",  2,  0,  MinimumRest,true);
	R.InitScenarios(STree.SubScenarios.back());
	R.AddIndex(0,STree.SubScenarios.back().size(),0,true);
    R.AddIndex(1,NumberOfBars);

    Variable NoR("nr",  2,  0,  1,true);
	NoR.InitScenarios(STree.SubScenarios.back());
	NoR.AddIndex(0,STree.SubScenarios.back().size(),0,true);
    NoR.AddIndex(1,NumberOfBars);

	stringstream mipout;
	mipout<<BaseName<<"_RC.lp";
	ofstream out(mipout.str().c_str());

	out<<"MINIMIZE\n";
	out << "obj\n";
	
	out<<"\nSUBJECT TO\n";

		//Minimize the number of queries
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+"<<CostPerUnit<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" ";
		 for (int b=0; b<NumberOfBars; b++)                      
	                out<<"+"<<CostPerBar<<F.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" ";
		for (int t=1; t<NumberOfStages; t++)		
			for (int b=0; b<NumberOfBars; b++)			
				out<<"+"<<BarChangeCost<<W[NumberOfStages-1].VarName<<"_"<<STree.SubScenarios.back()[s].substr(0, t)<<"_"<<b<<" ";
		out <<"-obj <= 0\n";
	}

	// Do not excced each bars/bins capacity
	//If bar is used set binary F variable
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
        for (int b=0; b<NumberOfBars; b++){             
            for (int t=0; t<NumberOfStages; t++){           
                for (int i=0; i<NumberOfItems; i++){
                	if(t==0){
                		out << "+"<< AI.VarName<<"_"<< b <<" ";
                		break;
                	}
                	else{
                    	out << "+"<< A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
                	}
                	
				}
            }
            out <<"-"<<NumberOfStages<<F.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= 0"<<endl;
        }
    }
    for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int b=0; b<NumberOfBars; b++){		
			for (int t=1; t<NumberOfStages; t++){		
				for (int i=0; i<NumberOfItems; i++){
					out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
				}
			}
			out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
		}
	}

	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
 		for (int b=0; b<NumberOfBars; b++){             
            for (int t=1; t<NumberOfStages; t++){           
                    for (int i=0; i<NumberOfItems; i++){
                            out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
                    }
            }
            out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" +" <<BarSizes[b]<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" >= "<<BarSizes[b]<<endl;
        }
    }

    for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int b=0; b<NumberOfBars; b++){             
	    	for (int t=1; t<NumberOfStages; t++){           
	            for (int i=0; i<NumberOfItems; i++){
	                out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
	            }
	    	}
	    	out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b<<" +"<<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
		}
	}

	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int b=0; b<NumberOfBars; b++){  
			out <<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" +" <<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= "<<MinimumRest<<endl;
		}
	}

	// Ausgewähltes Item muss auch zugeordnet werden
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int t=1; t<NumberOfStages; t++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int b=0; b<NumberOfBars; b++){		
					out << "+"<< A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
				}
				//out << " "<<i << " " <<STree.SubScenarios.back()[s][t-1] << " " << (STree.SubScenarios.back()[s].compare(t-1,1,to_string(i))) << endl; 
				if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios.back()[s], static_cast<std::size_t>(t-1), i)) out <<" = 1" <<endl;
				else out <<" = 0" <<endl; 
			}
		}
	}


	//Initial Item muss zugewiesen werden
	for (int b=0; b<NumberOfBars; b++){		
		out << "+"<< AI.VarName<<"_"<< b <<" ";
	}
	out << " = 1"<<endl;

	// Wechselvariablen belegen

	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int t=1; t<NumberOfStages; t++){		
			for (int b=0; b<NumberOfBars; b++){		
				for (int i=0; i<NumberOfItems; i++){		
					out << "+"<< A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
				}
				if(t==1){
					out << "-"<< AI.VarName<<"_"<< b << " ";
				}
				else{
					for (int i=0; i<NumberOfItems; i++){		
						out << "-"<< A[t-2].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t-1)<<"_"<< b << "_"<<  i <<" ";
					}
				}
				out <<"-"<<W[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<<b<< " <= 0 "<<endl;

	//			if(t==1){
	//				out << "+"<< AI.VarName<<"_0_"<< b << "_0 ";
	//			}
	//			else{
	//				for (int i=0; i<NumberOfItems; i++){		
	//					out << "+"<< A.VarName<<"_"<< t-1<<"_"<< b << "_"<<  i <<" ";
	//				}
	//			}
	//			for (int i=0; i<NumberOfItems; i++){		
	//				out << "-"<< A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
	//			}
	//			out <<"-"<<W.VarName<<"_"<<t<<"_"<<b<< " <= 0 "<<endl;
	//
			}
		}
	}
	/*
	//Pro Stufe genau ein Item
	for (int t=1; t<NumberOfStages; t++){	
		out <<"U_T"<<t<<": ";
		for (int i=0; i<NumberOfItems; i++){		
			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "=1" <<endl;
	}

	//Jedes Item genau A_i mal
	for (int i=0; i<NumberOfItems; i++){	
		out <<"U_I"<<i<<": ";
		for (int t=1; t<NumberOfStages; t++){	
			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "= "<<Partitions[RandPart][i] <<endl;
	}
*/
	out<<"BOUNDS\n";
	AI.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out,true);	
	R.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out,true);	
	F.Print(out,true);
	NoR.Print(out,true);
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
	AI.Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out);
	F.Print(out);
	NoR.Print(out);

	out<<"END\n";
	out.close();


	return;
}

