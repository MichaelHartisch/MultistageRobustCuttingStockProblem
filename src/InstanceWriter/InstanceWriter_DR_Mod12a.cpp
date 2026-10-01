#include "InstanceWriter.h"
#include <string>
#include <vector>
#include <iostream>
#include "Variable.h"

void InstanceWriter::writeDR_12a(DR_Holder DR){
	//std::cerr<<"Start writing DR_12a-File"<<std::endl;

	Variable AI("ai",  1,  0,  1);
	AI.AddIndex(0,NumberOfBars);

	vector <Variable> A;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("a_"+to_string(t+1),  2,  0,  1);
		Av.AddIndex(0,NumberOfBars);
		Av.AddIndex(1,NumberOfItems);
		A.push_back(Av);
	}

	vector <Variable> ATop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aT_"+to_string(t+1),  2,  -1000,  1000,true);
		Av.AddIndex(0,NumberOfBars);
		Av.AddIndex(1,NumberOfItems);
		ATop.push_back(Av);
	}

	Variable Z("z",  2,  0,  1);
	Z.AddIndex(0,NumberOfStages,1);
	Z.AddIndex(1,NumberOfItems);

	vector <Variable> ABot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aB_"+to_string(t+1),  2,  -1000,  1000);
		Av.AddIndex(0,NumberOfBars);
		Av.AddIndex(1,NumberOfItems);
		ABot.push_back(Av);
	}


	vector <Variable> zTop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("zT_"+to_string(t+1),  3,  0,  1);
		Av.AddIndex(0,NumberOfBars);
		Av.AddIndex(1,NumberOfItems);
		Av.AddIndex(2,P);	
		zTop.push_back(Av);
	}

	vector <Variable> zBot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("zB_"+to_string(t+1),  3,  0,  1);
		Av.AddIndex(0,NumberOfBars);
		Av.AddIndex(1,NumberOfItems);
		Av.AddIndex(2,P);	
		zBot.push_back(Av);
	}


	Variable F("f",  1,  0,  1);
    F.AddIndex(0,NumberOfBars,0);

    vector <Variable> W;
	for (int t=0;t<NumberOfStages;t++){
		Variable Wv("w_"+to_string(t+1),  1,  0,  1);
		Wv.AddIndex(0,NumberOfBars);
		W.push_back(Wv);
	}

	Variable R("r",  1,  0,  MinimumRest);
    R.AddIndex(0,NumberOfBars);

    Variable NoR("nr",  1,  0,  1);
    NoR.AddIndex(0,NumberOfBars);

	stringstream mipout;
	mipout<<BaseName<<"_DR12a.lp";
	ofstream out(mipout.str().c_str());
		double eps=1;
	out.precision(20);

	out<<"MAXIMIZE\n";

	//Minimize the number of queries
	for (int b=0; b<NumberOfBars; b++)			
		out<<"+"<<CostPerUnit<<R.VarName<<"_"<<b<<" ";
	 for (int b=0; b<NumberOfBars; b++)                      
                out<<"+"<<CostPerBar<<F.VarName<<"_"<<b<<" ";
	for (int t=1; t<NumberOfStages; t++)		
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+"<<BarChangeCost<<W[t-1].VarName<<"_"<<b<<" ";
	out <</*"-" << DR.opt<<*/"\n";
		out<<"\nSUBJECT TO\n";

	// Do not excced each bars/bins capacity
	//If bar is used set binary F variable
    for (int b=0; b<NumberOfBars; b++){             
        for (int t=0; t<NumberOfStages; t++){           
            for (int i=0; i<NumberOfItems; i++){
            	if(t==0){
            		out << "+"<< AI.VarName<<"_"<< b <<" ";
            		break;
            	}
            	else{
                	out << "+"<< A[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
            	}
            	
			}
        }
        out <<"-"<<F.VarName<<"_"<<b<<" >= 0"<<endl;	//Here:Changed for Maximization
    }

	for (int b=0; b<NumberOfBars; b++){		
		for (int t=1; t<NumberOfStages; t++){		
			for (int i=0; i<NumberOfItems; i++){
				out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
			}
		}
		out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){             
        for (int t=1; t<NumberOfStages; t++){           
                for (int i=0; i<NumberOfItems; i++){
                        out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
                }
        }
        out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " -"<<(BarSizes[b]-MinimumRest+1)<<NoR.VarName<<"_"<<b<<" >= 0"<<endl;
    }

	for (int b=0; b<NumberOfBars; b++){  
		out <<R.VarName<<"_"<<b <<" -" <<MinimumRest<<NoR.VarName<<"_"<<b<<" <= 0"<<endl;
	}

	// Ausgewähltes Item muss auch zugeordnet werden
	/*for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
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
	}*/


	//Initial Item muss zugewiesen werden
	for (int b=0; b<NumberOfBars; b++){		
		out << "+"<< AI.VarName<<"_"<< b <<" ";
		if(b==DR.FirstStageBar) out << " = 1"<<endl;
		else  out << " = 0"<<endl;
	}
	

	// Wechselvariablen belegen

	for (int t=1; t<NumberOfStages; t++){		
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){		
				out << "+"<< A[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
			}
			out <<"-"<<W[t-1].VarName<<"_"<<b<< " >= 0 "<<endl;

			if(t==1){
				out << "+"<< AI.VarName<<"_"<< b << " ";
			}
			else{
				for (int i=0; i<NumberOfItems; i++){		
					out << "+"<< A[t-2].VarName<<"_"<< b << "_"<<  i <<" ";
				}
			}
			out <<"+"<<W[t-1].VarName<<"_"<<b<< " <= 1 "<<endl;

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

	//y(xi^t)=1 => yTop(xi^t) -yBot(xi^t)<=0
	
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				out << A[t-1].VarName<<"_"<< b << "_"<<  i <<"=1 -> ";
				out << ATop[t-1].VarName<<"_"<< b << "_"<<  i <<"-";
				out << ABot[t-1].VarName<<"_"<< b << "_"<<  i <<" <= 0"<<endl;
			}
		}
	}	

	//y(xi^t)=0 => yTop(xi^t) -yBot(xi^t)>=eps
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				out << A[t-1].VarName<<"_"<< b << "_"<<  i <<"=0 -> ";
				out << ATop[t-1].VarName<<"_"<< b << "_"<<  i <<" -";
				out << ABot[t-1].VarName<<"_"<< b << "_"<<  i <<" >= " <<eps <<endl;
			}
		}
	}	


	/*
	Variable yTop("yT",  5,  -1000,  1000);
	yTop.AddIndex(0,NumberOfStages);		//In each stage
	yTop.AddIndex(1,NumberOfBars);			//We need Variables describing the function for a_b_i 
	yTop.AddIndex(2,NumberOfItems);			
	yTop.AddIndex(3,P);						
	yTop.AddIndex(4,NumberOfItems*(t+1));		//Which depend on all the previous stages 
*/



	//ATop(xi^t) >= yBar_p_t*xi
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int p=0;p<P;p++){
					out << ATop[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
					for (int stages=0;stages<t;stages++){
						for (int ii=0; ii<NumberOfItems; ii++){
							if(abs(DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii])>0){
								if(-DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii]>=0) out <<"+";
								out.precision(10);
								out << -DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii]<<Z.VarName<<"_"<<stages+1<<"_"<<ii << " ";
							}
						}
					}
					out << " >= "<< DR.yTopConst[t-1][b][i][p]<<endl;
				}
			}
		}
	}

	//ABot(xi^t) >= yBot_p_t*xi
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int p=0;p<P;p++){
					out << ABot[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
					for (int stages=0;stages<t;stages++){
						for (int ii=0; ii<NumberOfItems; ii++){
							if(abs(DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii])>0){
								if(-DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii]>=0) out <<"+";
								out << -DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii]<<Z.VarName<<"_"<<stages+1<<"_"<<ii << " ";
							}
						}
					}
					out << " >= "<< DR.yBotConst[t-1][b][i][p]<<endl;
				}
			}
		}
	}

	// zTop_p_t = 1 => ATop(xi^t) <= yBar_p_t*xi
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int p=0;p<P;p++){
					out << zTop[t-1].VarName<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> " << ATop[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
					for (int stages=0;stages<t;stages++){
						for (int ii=0; ii<NumberOfItems; ii++){
							if(fabs(DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii])>0){
								if(-DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii]>0) out <<"+";
								out << -DR.yTop[t-1][b][i][p][(stages)*NumberOfItems+ii]<<Z.VarName<<"_"<<stages+1<<"_"<<ii << " ";
							}
						}
					}
					out << " <= "<< DR.yTopConst[t-1][b][i][p]<<endl;
				}
			}
		}
	}

	// zBot_p_t = 1 => ABot(xi^t) <= yBot_p_t*xi
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int p=0;p<P;p++){
					out << zBot[t-1].VarName<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> " << ABot[t-1].VarName<<"_"<< b << "_"<<  i <<" ";
					for (int stages=0;stages<t;stages++){
						for (int ii=0; ii<NumberOfItems; ii++){
							if(fabs(DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii])>0){
								if(-DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii]>0) out <<"+";
								out << -DR.yBot[t-1][b][i][p][(stages)*NumberOfItems+ii]<<Z.VarName<<"_"<<stages+1<<"_"<<ii << " ";
							}
						}
					}
					out << " <= "<< DR.yBotConst[t-1][b][i][p]<<endl;
				}
			}
		}
	}

	//sum zT_p_t =1 
	for (int t=1; t<NumberOfStages; t++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){
				for (int p=0;p<P;p++){
					out << " +"<<zTop[t-1].VarName<<"_"<< b << "_"<<  i << "_"<<  p;	
				}
				out << " =1"<<endl;
				for (int p=0;p<P;p++){
					out << " +"<< zBot[t-1].VarName<<"_"<< b << "_"<<  i << "_"<<  p;	
				}
				out << " =1"<<endl;
			}
		}
	}

	
	//Pro Stufe genau ein Item
	for (int t=1; t<NumberOfStages; t++){	
		for (int i=0; i<NumberOfItems; i++){		
			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "=1" <<endl;
	}

	//Jedes Item genau A_i mal
	for (int i=0; i<NumberOfItems; i++){	
		for (int t=1; t<NumberOfStages; t++){	
			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "= "<<Partitions[RandPart][i] <<endl;
	}

	out<<"BOUNDS\n";
	AI.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out,true);	
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out,true);	
	F.Print(out,true);
	NoR.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++){
		ATop[t].Print(out,true,true);
		ABot[t].Print(out,true,true);
		zTop[t].Print(out,true);
		zBot[t].Print(out,true);
		/*yTop[t].Print(out,true);
		yBot[t].Print(out,true);*/
	}
	Z.Print(out,true);

	
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
	AI.Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		zTop[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		zBot[t].Print(out);
	Z.Print(out);
	F.Print(out);
	NoR.Print(out);

	out<<"END\n";
	out.close();


	return;
}

