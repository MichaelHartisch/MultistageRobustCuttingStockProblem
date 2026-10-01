#include "InstanceWriter.h"
#include <string>
#include <vector>
#include <iostream>
#include "Variable.h"
#include "ScenarioEncoding.h"

void InstanceWriter::writeDR_basic(double Bound){
	//std::cerr<<"Start writing DR-File"<<std::endl;
	double eps=1;
	bool MoreThanOneP = true;//((P>=2)?false:true);
	Variable AI("ai",  1,  0,  1);
	AI.AddIndex(0,NumberOfBars);

	vector <Variable> A;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("a",  3,  0,  1,true);
		Av.InitScenarios(FullTree.SubScenarios[t]);
		Av.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		A.push_back(Av);
	}

	vector <Variable> ATop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aT",  3,  -1000,  1000,true);
		Av.InitScenarios(FullTree.SubScenarios[t]);
		Av.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		ATop.push_back(Av);
	}

	vector <Variable> ABot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aB",  3,  -1000,  1000,true);
		Av.InitScenarios(FullTree.SubScenarios[t]);
		Av.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		ABot.push_back(Av);
	}

	vector <Variable> yTop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable yTo("yT_"+to_string(t),  4,  -1000,  1000);
		yTo.AddIndex(0,NumberOfBars);			//We need Variables describing the function for a_b_i 
		yTo.AddIndex(1,NumberOfItems);			
		yTo.AddIndex(2,P);						
		yTo.AddIndex(3,NumberOfItems*(t+1));		//Which depend on all the previous stages 
		yTop.push_back(yTo);
	}
	vector <Variable> yTopCon;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable yTo("yTc_"+to_string(t),  3,  -1000,  1000);
		yTo.AddIndex(0,NumberOfBars);			//We need Variables describing the function for a_b_i 
		yTo.AddIndex(1,NumberOfItems);			
		yTo.AddIndex(2,P);						
		yTopCon.push_back(yTo);
	}

	vector <Variable> yBot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable yTo("yB_"+to_string(t),  4,  -1000,  1000);
		yTo.AddIndex(0,NumberOfBars);			//We need Variables describing the function for a_b_i 
		yTo.AddIndex(1,NumberOfItems);			
		yTo.AddIndex(2,P);						
		yTo.AddIndex(3,NumberOfItems*(t+1));		//Which depend on all the previous stages 
		yBot.push_back(yTo);
	}

	vector <Variable> yBotCon;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable yTo("yBc_"+to_string(t),  3,  -1000,  1000);
		yTo.AddIndex(0,NumberOfBars);			//We need Variables describing the function for a_b_i 
		yTo.AddIndex(1,NumberOfItems);			
		yTo.AddIndex(2,P);						
		yBotCon.push_back(yTo);
	}


	vector <Variable> zTop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("zT",  4,  0,  1,true);
		Av.InitScenarios(FullTree.SubScenarios[t]);
		Av.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		Av.AddIndex(3,P);	
		zTop.push_back(Av);
	}

	vector <Variable> zBot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("zB",  4,  0,  1,true);
		Av.InitScenarios(FullTree.SubScenarios[t]);
		Av.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		Av.AddIndex(3,P);	
		zBot.push_back(Av);
	}


	Variable F("f",  2,  0,  1,true);
	F.InitScenarios(FullTree.SubScenarios.back());
	F.AddIndex(0,FullTree.SubScenarios.back().size(),0,true);
    F.AddIndex(1,NumberOfBars,0);

    vector <Variable> W;
	for (int t=0;t<NumberOfStages;t++){
		Variable Wv("w",  2,  0,  1,true);
		Wv.InitScenarios(FullTree.SubScenarios[t]);
		Wv.AddIndex(0,FullTree.SubScenarios[t].size(),0,true);
		Wv.AddIndex(1,NumberOfBars);
		W.push_back(Wv);
	}

	Variable R("r",  2,  0,  MinimumRest,true);
	R.InitScenarios(FullTree.SubScenarios.back());
	R.AddIndex(0,FullTree.SubScenarios.back().size(),0,true);
    R.AddIndex(1,NumberOfBars);

    Variable NoR("nr",  2,  0,  1,true);
	NoR.InitScenarios(FullTree.SubScenarios.back());
	NoR.AddIndex(0,FullTree.SubScenarios.back().size(),0,true);
    NoR.AddIndex(1,NumberOfBars);

	stringstream mipout;
	mipout<<BaseName<<"_DR_basic.lp";
	ofstream out(mipout.str().c_str());

	out<<"MINIMIZE\n";
	out << "obj\n";
	
	out<<"\nSUBJECT TO\n";
	out << "obj >= "<<Bound <<endl;
		//Minimize the number of queries
	for(int s=0;s<FullTree.SubScenarios.back().size();s++){
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+"<<CostPerUnit<<R.VarName<<"_"<<FullTree.SubScenarios.back()[s]<<"_"<<b<<" ";							//Add Variable
		 for (int b=0; b<NumberOfBars; b++)                      
	                out<<"+"<<CostPerBar<<F.VarName<<"_"<<FullTree.SubScenarios.back()[s]<<"_"<<b<<" ";					//Add Variable
		for (int t=1; t<NumberOfStages; t++)		
			for (int b=0; b<NumberOfBars; b++)			
				out<<"+"<<BarChangeCost<<W[t-1].VarName<<"_"<<FullTree.SubScenarios.back()[s].substr(0, t)<<"_"<<b<<" ";	//Add Variable
		out <<"-obj <= 0\n";
	}

	/*// Do not excced each bars/bins capacity
	//If bar is used set binary F variable
	for(int s=0;s<STree.SubScenarios.back().size();s++){
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
    for(int s=0;s<STree.SubScenarios.back().size();s++){
		for (int b=0; b<NumberOfBars; b++){		
			for (int t=1; t<NumberOfStages; t++){		
				for (int i=0; i<NumberOfItems; i++){
					out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
				}
			}
			out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
		}
	}

	for(int s=0;s<STree.SubScenarios.back().size();s++){
 		for (int b=0; b<NumberOfBars; b++){             
            for (int t=1; t<NumberOfStages; t++){           
                    for (int i=0; i<NumberOfItems; i++){
                            out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
                    }
            }
            out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" +" <<BarSizes[b]<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" >= "<<BarSizes[b]<<endl;
        }
    }

    for(int s=0;s<STree.SubScenarios.back().size();s++){
		for (int b=0; b<NumberOfBars; b++){             
	    	for (int t=1; t<NumberOfStages; t++){           
	            for (int i=0; i<NumberOfItems; i++){
	                out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
	            }
	    	}
	    	out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b<<" +"<<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
		}
	}

	for(int s=0;s<STree.SubScenarios.back().size();s++){
		for (int b=0; b<NumberOfBars; b++){  
			out <<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" +" <<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= "<<MinimumRest<<endl;
		}
	}

	// Ausgewähltes Item muss auch zugeordnet werden
	for(int s=0;s<STree.SubScenarios.back().size();s++){
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

*/
	//Initial Item muss zugewiesen werden
	for (int b=0; b<NumberOfBars; b++){		
		out << "+"<< AI.VarName<<"_"<< b <<" ";
	}
	out << " = 1"<<endl;
/*
	// Wechselvariablen belegen

	for(int s=0;s<STree.SubScenarios.back().size();s++){
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

	//y(xi^t)=1 => yTop(xi^t) -yBot(xi^t)<=0
	
	for (int t=1; t<NumberOfStages; t++){
		for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
			for (int b=0; b<NumberOfBars; b++){		
				for (int i=0; i<NumberOfItems; i++){
					out << A[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<"=1 -> ";
					if(MoreThanOneP) {
						out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
						out << " -" <<ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
					}
					else{
						for (int stages=0;stages<t;stages++){
							for (int ii=0; ii<NumberOfItems; ii++){
								if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
									&& (t-ConsideredStages) <= stages) 
										out <<"+"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0<<"_"<<(stages)*NumberOfItems+ii << " ";
							}
						}
						out << "+" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0;
					}
					out <<" <= 0"<<endl;
				}
			}
		}
	}	

	//y(xi^t)=0 => yTop(xi^t) -yBot(xi^t)>=eps
	for (int t=1; t<NumberOfStages; t++){
		for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
			for (int b=0; b<NumberOfBars; b++){		
				for (int i=0; i<NumberOfItems; i++){
					out << A[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<"=0 -> ";
					if(MoreThanOneP){
						out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i ;
					 	out << " -"<<ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
					}
					else{
					for (int stages=0;stages<t;stages++){
							for (int ii=0; ii<NumberOfItems; ii++){
								if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
									&& (t-ConsideredStages) <= stages) 
										out <<"+"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0<<"_"<<(stages)*NumberOfItems+ii << " ";
							}
						}
						out << "+" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0;
					}
					out <<" >= " <<eps <<endl;
				}
			}
		}
	}	


	
//	Variable yTop("yT",  5,  -1000,  1000);
//	yTop.AddIndex(0,NumberOfStages);		//In each stage
//	yTop.AddIndex(1,NumberOfBars);			//We need Variables describing the function for a_b_i 
//	yTop.AddIndex(2,NumberOfItems);			
//	yTop.AddIndex(3,P);						
//	yTop.AddIndex(4,NumberOfItems*(t+1));		//Which depend on all the previous stages 



	//ATop(xi^t) >= yBar_p_t*xi
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						for (int p=0;p<P;p++){
							out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
							for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
										&& (t-ConsideredStages) <= stages) 
											out <<"-"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
								}
							}
							out << "-" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<< " >= 0"<<endl;
						}
					}
				}
			}
		}
	}

	//ABot(xi^t) >= yBot_p_t*xi
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						for (int p=0;p<P;p++){
							out << ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
							for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
										&& (t-ConsideredStages) <= stages) 
											out <<"-"<< yBot[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
								}
							}
							out << "-" <<yBotCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<< " >= 0"<<endl;
						}
					}
				}
			}
		}
	}

	// zTop_p_t = 1 => ATop(xi^t) <= yBar_p_t*xi
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						for (int p=0;p<P;p++){
							out << zTop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> " << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
							for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
										&& (t-ConsideredStages) <= stages) 
											out <<"-"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
								}
							}
							out << "-" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<< " <= 0"<<endl;
						}
					}
				}
			}
		}
	}

	// zBot_p_t = 1 => ABot(xi^t) <= yBot_p_t*xi
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						for (int p=0;p<P;p++){
							out << zBot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> " << ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
							for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
										&& (t-ConsideredStages) <= stages) 
											out <<"-"<< yBot[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
								}
							}
							out << "-" <<yBotCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<< " <= 0"<<endl;
						}
					}
				}
			}
		}
	}

	//sum zT_p_t =1 
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						for (int p=0;p<P;p++){
							out << " +"<<zTop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p;	
						}
						out << " =1"<<endl;
						for (int p=0;p<P;p++){
							out << " +"<< zBot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p;	
						}
						out << " =1"<<endl;
					}
				}
			}
		}
	}


//	//Pro Stufe genau ein Item
//	for (int t=1; t<NumberOfStages; t++){	
//		out <<"U_T"<<t<<": ";
//		for (int i=0; i<NumberOfItems; i++){		
//			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
//		}
//		out << "=1" <<endl;
//	}
//
//	//Jedes Item genau A_i mal
//	for (int i=0; i<NumberOfItems; i++){	
//		out <<"U_I"<<i<<": ";
//		for (int t=1; t<NumberOfStages; t++){	
//			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
//		}
//		out << "= "<<Partitions[RandPart][i] <<endl;
//	}

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
	/*for (int t=0;t<NumberOfStages-1;t++){
		if(MoreThanOneP){ 
			ATop[t].Print(out,true,true);
			ABot[t].Print(out,true,true);
			zTop[t].Print(out,true);
			zBot[t].Print(out,true);
			yBot[t].Print(out,true,true);
			yBotCon[t].Print(out,true,true);
		}
		yTop[t].Print(out,true,true);
		yTopCon[t].Print(out,true,true);


	}
		*/	
	
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
	AI.Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out);
	//for (int t=0;t<NumberOfStages-1;t++)
	//	W[t].Print(out);
	//for (int t=0;t<NumberOfStages-1;t++)
	//	if(MoreThanOneP) zTop[t].Print(out);
	//for (int t=0;t<NumberOfStages-1;t++)
	//	if(MoreThanOneP) zBot[t].Print(out);
	F.Print(out);
	NoR.Print(out);

	out<<"END\n";
	out.close();


	return;
}

