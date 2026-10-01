#include "InstanceWriter.h"
#include <string>
#include <vector>
#include <iostream>
#include "Variable.h"
#include "CplexSolver.h"
#include "ScenarioEncoding.h"

void InstanceWriter::DR_prepare(){
	//std::cerr<<"Start writing DR-File"<<std::endl;
	double Bound=0;
	double eps=1;
	bool MoreThanOneP = true;//((P>=2)?false:true);
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

	vector <Variable> ATop;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aT",  3,  -1000,  1000,true);
		Av.InitScenarios(STree.SubScenarios[t]);
		Av.AddIndex(0,STree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		ATop.push_back(Av);
	}

	vector <Variable> ABot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("aB",  3,  -1000,  1000,true);
		Av.InitScenarios(STree.SubScenarios[t]);
		Av.AddIndex(0,STree.SubScenarios[t].size(),0,true);
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
		Av.InitScenarios(STree.SubScenarios[t]);
		Av.AddIndex(0,STree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		Av.AddIndex(3,P);	
		zTop.push_back(Av);
	}

	vector <Variable> zBot;
	for (int t=0;t<NumberOfStages-1;t++){
		Variable Av("zB",  4,  0,  1,true);
		Av.InitScenarios(STree.SubScenarios[t]);
		Av.AddIndex(0,STree.SubScenarios[t].size(),0,true);
		Av.AddIndex(1,NumberOfBars);
		Av.AddIndex(2,NumberOfItems);
		Av.AddIndex(3,P);	
		zBot.push_back(Av);
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

	/*stringstream mipout;
	mipout<<BaseName<<"_DR.lp";
	ofstream out(mipout.str().c_str());

	out<<"MINIMIZE\n";
	out << "obj\n";
	
	out<<"\nSUBJECT TO\n";
	out << "obj >= "<<Bound <<endl;
		//Minimize the number of queries
	for(int s=0;s<STree.SubScenarios.back().size();s++){
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+"<<CostPerUnit<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" ";
		 for (int b=0; b<NumberOfBars; b++)                      
	                out<<"+"<<CostPerBar<<F.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" ";
		for (int t=1; t<NumberOfStages; t++)		
			for (int b=0; b<NumberOfBars; b++)			
				out<<"+"<<BarChangeCost<<W[t-1].VarName<<"_"<<STree.SubScenarios.back()[s].substr(0, t)<<"_"<<b<<" ";
		out <<"-obj <= 0\n";
	}*/
	// Do not excced each bars/bins capacity
	//If bar is used set binary F variable
	for(int s=0;s<STree.SubScenarios.back().size();s++){
		if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
	        for (int b=0; b<NumberOfBars; b++){            
	        	row NewRow; 
	            for (int t=0; t<NumberOfStages; t++){           
	                for (int i=0; i<NumberOfItems; i++){
	                	if(t==0){
	                		NewRow.Coef.push_back(1);
	                		string Name=AI.VarName+ "_" + std::to_string(b);
	                		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	                		//out << "+"<< AI.VarName<<"_"<< b <<" ";
	                		break;
	                	}
	                	else{
	                		NewRow.Coef.push_back(1);
	                		string Name=A[t-1].VarName+ "_" +STree.SubScenarios.back()[s].substr(0, t)+ "_" + std::to_string(b) + "_" + std::to_string(i) ;
	                		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	                    	//out << "+"<< A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
	                	}
	                	
					}
	            }
	            NewRow.Coef.push_back(-NumberOfStages);
	      		string Name=F.VarName+ "_" +STree.SubScenarios.back()[s]+ "_" +std::to_string(b) ;
	      		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	            //out <<"-"<<NumberOfStages<<F.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<" <= 0"<<endl;
	       		NewRow.active=false;
	        	NewRow.sense='L';
	        	NewRow.rhs=0;
	        	NewRow.IsIndicator=false;
	        	Solver->LazyRows.push_back(NewRow);
	         	//std::cerr << "HERE: Level" << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << "Node" << STree.SubScenariosToNode.back()[s] << endl;
	        	//STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;

	        	//ONLY LINKED TO Main Sceanrio! 
        	}
        }
    }


    for(int s=0;s<STree.SubScenarios.back().size();s++){
    	if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
			for (int b=0; b<NumberOfBars; b++){		
				row NewRow; 
				for (int t=1; t<NumberOfStages; t++){		
					for (int i=0; i<NumberOfItems; i++){
						NewRow.Coef.push_back(LengthOfItems[i]);
	                	string Name=A[t-1].VarName+ "_" + STree.SubScenarios.back()[s].substr(0, t)+ "_" + std::to_string(b) + "_" + std::to_string(i);
	                	NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
						//out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
					}
				}
				NewRow.Coef.push_back(LengthOfInitial);
	      		string Name=AI.VarName+ "_" + std::to_string(b);
	      		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	      		NewRow.Coef.push_back(1);
	      		Name=R.VarName+ "_" +STree.SubScenarios.back()[s]+ "_" + std::to_string(b);
	      		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
				//out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << " +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
				NewRow.active=false;
	        	NewRow.sense='L';
	        	NewRow.rhs=BarSizes[b];
	        	NewRow.IsIndicator=false;
	        	Solver->LazyRows.push_back(NewRow);
	        	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
	     //   	STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
			}
		}
	}

	for(int s=0;s<STree.SubScenarios.back().size();s++){
		if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
	 		for (int b=0; b<NumberOfBars; b++){     
	 			row NewRow; 
	        
	            for (int t=1; t<NumberOfStages; t++){           
	                    for (int i=0; i<NumberOfItems; i++){
	                    	NewRow.Coef.push_back(LengthOfItems[i]);
	                		string VName=A[t-1].VarName+ "_" + STree.SubScenarios.back()[s].substr(0, t)+ "_" + std::to_string(b) + "_" + std::to_string(i);
	                		NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(VName.c_str())));
	                        //out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
	                    }
	            }
	            NewRow.Coef.push_back(LengthOfInitial);
	            string VName=AI.VarName+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(VName.c_str())));
	            NewRow.Coef.push_back(1);
	            VName=R.VarName+"_"+STree.SubScenarios.back()[s]+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(VName.c_str())));
	            NewRow.Coef.push_back(BarSizes[b]);
	            VName=NoR.VarName+"_"+STree.SubScenarios.back()[s]+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(VName.c_str())));
	            //out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b << 
	            //" +"<<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<
	            //" +" <<BarSizes[b]<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<
	            //" >= "<<BarSizes[b]<<endl;
	            NewRow.active=false;
	        	NewRow.sense='G';
	        	NewRow.rhs=BarSizes[b];
	        	NewRow.IsIndicator=false;
	        	Solver->LazyRows.push_back(NewRow);
	        	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
	        	//STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
	        }
	    }
    }
    
    for(int s=0;s<STree.SubScenarios.back().size();s++){
    	if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
			for (int b=0; b<NumberOfBars; b++){   
			 	row NewRow; 
		    	for (int t=1; t<NumberOfStages; t++){           
		            for (int i=0; i<NumberOfItems; i++){
		            	NewRow.Coef.push_back(LengthOfItems[i]);
	                	string Name=A[t-1].VarName+"_"+ STree.SubScenarios.back()[s].substr(0, t)+"_"+ std::to_string(b) + "_"+ std::to_string(i);
	                	NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
		                //out << "+"<< LengthOfItems[i]<<A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
		            }
		    	}
		    	NewRow.Coef.push_back(LengthOfInitial);
	            string Name=AI.VarName+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	            NewRow.Coef.push_back(MinimumRest);
	            Name=NoR.VarName+"_"+STree.SubScenarios.back()[s]+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
		    	//out <<"+" << LengthOfInitial << AI.VarName<<"_"<<b<<
		    	//" +"<<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<
		    	//" <= "<<BarSizes[b]<<endl;
		    	NewRow.active=false;
	        	NewRow.sense='L';
	        	NewRow.rhs=BarSizes[b];
	        	NewRow.IsIndicator=false;
	        	Solver->LazyRows.push_back(NewRow);
	        	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
	        	//STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
			}
		}
	}
	

	for(int s=0;s<STree.SubScenarios.back().size();s++){
		if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
			for (int b=0; b<NumberOfBars; b++){  
				row NewRow; 
				NewRow.Coef.push_back(1);
	            string Name=R.VarName+"_"+STree.SubScenarios.back()[s]+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	            NewRow.Coef.push_back(MinimumRest);
	            Name=NoR.VarName+"_"+STree.SubScenarios.back()[s]+"_"+ std::to_string(b);
	            NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
				//out <<R.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b <<
				//" +" <<MinimumRest<<NoR.VarName<<"_"<<STree.SubScenarios.back()[s]<<"_"<<b<<
				//" <= "<<MinimumRest<<endl;
				NewRow.active=false;
	        	NewRow.sense='L';
	        	NewRow.rhs=MinimumRest;
	        	NewRow.IsIndicator=false;
	        	Solver->LazyRows.push_back(NewRow);
	        	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
	        	STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
			}
		}
	}

	// Ausgewähltes Item muss auch zugeordnet werden
	for(int s=0;s<STree.SubScenarios.back().size();s++){
		if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
			for (int t=1; t<NumberOfStages; t++){		
				for (int i=0; i<NumberOfItems; i++){
					row NewRow; 
					for (int b=0; b<NumberOfBars; b++){	
						NewRow.Coef.push_back(1);
	                	string Name=A[t-1].VarName+"_"+ STree.SubScenarios.back()[s].substr(0, t)+"_"+ std::to_string(b) + "_"+ std::to_string(i);
	                	NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
						//out << "+"<< A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
					}
					NewRow.active=false;
	        		NewRow.sense='E';
	        		NewRow.IsIndicator=false;
					if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios.back()[s], static_cast<std::size_t>(t-1), i)){
						NewRow.rhs=1;
						//out <<" = 1" <<endl;	
					} 
					else{
						NewRow.rhs=0; 
						//out <<" = 0" <<endl; 
					}

		        	Solver->LazyRows.push_back(NewRow);
		        	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
		        	STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
		        	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
				}
			}
		}
	}


//NICHT REIN! 
/*	//Initial Item muss zugewiesen werden
	row NewRowIni; 

	for (int b=0; b<NumberOfBars; b++){		
		NewRowIni.Coef.push_back(1);
        string NameIni=AI.VarName+"_"+ std::to_string(b);
        NewRowIni.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(NameIni.c_str())));	
		//out << "+"<< AI.VarName<<"_"<< b <<" ";
	}
	//out << " = 1"<<endl;
	NewRowIni.active=false;
	NewRowIni.sense='E';
	NewRowIni.rhs=1;
	NewRowIni.IsIndicator=false;
	Solver->LazyRows.push_back(NewRowIni);
	//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
	STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
	//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
*/
	// Wechselvariablen belegen

	for(int s=0;s<STree.SubScenarios.back().size();s++){
		if(STree.CurrentTree[STree.SubScenariosToNode.back()[s]].IsNew){
			for (int t=1; t<NumberOfStages; t++){		
				for (int b=0; b<NumberOfBars; b++){		
					row NewRow; 
					for (int i=0; i<NumberOfItems; i++){	
						NewRow.Coef.push_back(1);
	        			string Name=A[t-1].VarName+"_"+ STree.SubScenarios.back()[s].substr(0, t) + "_"+std::to_string(b)+ "_"+std::to_string(i);
	       				NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
						//out << "+"<< A[t-1].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t)<<"_"<< b << "_"<<  i <<" ";
					}
					if(t==1){
						NewRow.Coef.push_back(-1);
	        			string Name=AI.VarName+ "_"+std::to_string(b);
						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
						//out << "-"<< AI.VarName<<"_"<< b << " ";
					}
					else{
						for (int i=0; i<NumberOfItems; i++){		
							NewRow.Coef.push_back(-1);
	        				string Name=A[t-2].VarName+"_"+ STree.SubScenarios.back()[s].substr(0, t-1) + "_"+std::to_string(b)+ "_"+std::to_string(i);
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
							//out << "-"<< A[t-2].VarName<<"_"<<  STree.SubScenarios.back()[s].substr(0, t-1)<<"_"<< b << "_"<<  i <<" ";
						}
					}
					NewRow.Coef.push_back(-1);
	        		string Name=W[t-1].VarName+"_"+ STree.SubScenarios.back()[s].substr(0, t) + "_"+std::to_string(b);
	       			NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
					//out <<"-"<<W[t-1].VarName<<"_"<< STree.SubScenarios.back()[s].substr(0, t)<<"_"<<b<< 
					//" <= 0 "<<endl;
					NewRow.active=false;
					NewRow.sense='L';
					NewRow.rhs=0;
					NewRow.IsIndicator=false;
					Solver->LazyRows.push_back(NewRow);
					//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode.back()[s]].level << " Node "<<STree.SubScenariosToNode.back()[s] << endl;
					//STree.CurrentTree[STree.SubScenariosToNode.back()[s]].RowIndices.push_back(Solver->LazyRows.size()-1);
					//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;

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
	}

	//y(xi^t)=1 => yTop(xi^t) -yBot(xi^t)<=0
	
	for (int t=1; t<NumberOfStages; t++){
		for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
			if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						row NewRow; 
		       			string Name=A[t-1].VarName+"_"+ STree.SubScenarios[t-1][s] + "_"+std::to_string(b)+ "_"+std::to_string(i);
						NewRow.IndicatorVariable=Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str()));
						NewRow.complemented=false;
						//out << A[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<"=1 -> ";
						if(MoreThanOneP) {
							NewRow.Coef.push_back(1);
	        				string Name=ATop[t-1].VarName+"_"+  STree.SubScenarios[t-1][s] + "_"+std::to_string(b)+ "_"+std::to_string(i);
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
							//out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
							
							NewRow.Coef.push_back(-1);
	        				Name=ABot[t-1].VarName+"_"+  STree.SubScenarios[t-1][s] + "_"+std::to_string(b)+ "_"+std::to_string(i);
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
							//out << " -" <<ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
						}
						else{
							for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii) && (t-ConsideredStages) <= stages){
										NewRow.Coef.push_back(1);
	    								string Name=yTop[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_0_"+std::to_string((stages)*NumberOfItems+ii);
	   									NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
										//out <<"+"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0<<"_"<<(stages)*NumberOfItems+ii << " ";
									}
								}
							}
							NewRow.Coef.push_back(1);
	        				Name=yTopCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_0";
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
							//out << "+" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0;
						}
						//out <<" <= 0"<<endl;
						NewRow.active=false;
						NewRow.sense='L';
						NewRow.rhs=0;
						NewRow.IsIndicator=true;
						Solver->LazyRows.push_back(NewRow);
						//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
						STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
						//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
					}
				}
			}
		}
	}	

	//y(xi^t)=0 => yTop(xi^t) -yBot(xi^t)>=eps
	for (int t=1; t<NumberOfStages; t++){
		for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
			if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
				for (int b=0; b<NumberOfBars; b++){		
					for (int i=0; i<NumberOfItems; i++){
						row NewRow;
						string Name=A[t-1].VarName+"_"+ STree.SubScenarios[t-1][s] + "_"+std::to_string(b)+ "_"+std::to_string(i);
						NewRow.IndicatorVariable=Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str()));
						NewRow.complemented=true;

						//out << A[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<"=0 -> ";
						if(MoreThanOneP){
							NewRow.Coef.push_back(1);
	        				Name=ATop[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
							//out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i ;

							NewRow.Coef.push_back(-1);
	        				Name=ABot[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       					NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
						 	//out << " -"<<ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i;
						}
						else{
						for (int stages=0;stages<t;stages++){
								for (int ii=0; ii<NumberOfItems; ii++){
									if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
										&& (t-ConsideredStages) <= stages){
										NewRow.Coef.push_back(1);
	    								string Name=yTop[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_0_"+std::to_string((stages)*NumberOfItems+ii);
	   									NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
										//out <<"+"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0<<"_"<<(stages)*NumberOfItems+ii << " ";
	   								}
								}
							}
							NewRow.Coef.push_back(1);
							string Name=yTopCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_0";
	   						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
							//out << "+" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<0;
						}
						//out <<" >= " <<eps <<endl;
						NewRow.active=false;
						NewRow.sense='G';
						NewRow.rhs=eps;
						NewRow.IsIndicator=true;
						Solver->LazyRows.push_back(NewRow);
						//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
						STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
						//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
					}
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



	//Aop(xi^t) >= yBar_p_t*xi
	if(MoreThanOneP){
		for (int t=1; t<NumberOfStages; t++){
			for(int s=0;s<STree.SubScenarios[t-1].size();s++){	
				if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
					for (int b=0; b<NumberOfBars; b++){		
						for (int i=0; i<NumberOfItems; i++){
							for (int p=0;p<P;p++){
								row NewRow;
								NewRow.Coef.push_back(1);
	        					string Name=ATop[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));	
								//out << ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
								for (int stages=0;stages<t;stages++){
									for (int ii=0; ii<NumberOfItems; ii++){
										if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
											&& (t-ConsideredStages) <= stages){
										NewRow.Coef.push_back(-1);
	    								string Name=yTop[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p)+"_"+std::to_string((stages)*NumberOfItems+ii);
	   									NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
										//out <<"-"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
										}
									}
								}
								NewRow.Coef.push_back(-1);
	    						Name=yTopCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	   							NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << "-" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<
								 //" >= 0"<<endl;
	   							NewRow.active=false;
								NewRow.sense='G';
								NewRow.rhs=0;
								NewRow.IsIndicator=false;
								Solver->LazyRows.push_back(NewRow);
								//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
								STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
								//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
							}
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
				if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
					for (int b=0; b<NumberOfBars; b++){		
						for (int i=0; i<NumberOfItems; i++){
							for (int p=0;p<P;p++){
								row NewRow;
								NewRow.Coef.push_back(1);
	        					string Name=ABot[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
								for (int stages=0;stages<t;stages++){
									for (int ii=0; ii<NumberOfItems; ii++){
										if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
											&& (t-ConsideredStages) <= stages){
											NewRow.Coef.push_back(-1);
	    									string Name=yBot[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p)+"_"+std::to_string((stages)*NumberOfItems+ii);
	   										NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
											//out <<"-"<< yBot[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
										}
									}
								}
								NewRow.Coef.push_back(-1);
	    						Name=yBotCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	   							NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << "-" <<yBotCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<
								// " >= 0"<<endl;
	   							NewRow.active=false;
								NewRow.sense='G';
								NewRow.rhs=0;
								NewRow.IsIndicator=false;
								Solver->LazyRows.push_back(NewRow);
								//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
								STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
								//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
							}
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
				if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
					for (int b=0; b<NumberOfBars; b++){		
						for (int i=0; i<NumberOfItems; i++){
							for (int p=0;p<P;p++){
								row NewRow;
	        					string Name=zTop[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i)
	        								+ "_"+std::to_string(p);
								NewRow.IndicatorVariable=Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str()));
								NewRow.complemented=false;							
								//out << zTop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> " 
								NewRow.Coef.push_back(1);
	        					Name=ATop[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//<< ATop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
								for (int stages=0;stages<t;stages++){
									for (int ii=0; ii<NumberOfItems; ii++){
										if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
											&& (t-ConsideredStages) <= stages){
											NewRow.Coef.push_back(-1);
	    									string Name=yTop[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p)+"_"+std::to_string((stages)*NumberOfItems+ii);
	   										NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
	   										//out <<"-"<< yTop[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
										}
									}
								}
								NewRow.Coef.push_back(-1);
	        					Name=yTopCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << "-" <<yTopCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<
								// " <= 0"<<endl;
								NewRow.active=false;
								NewRow.sense='L';
								NewRow.rhs=0;
								NewRow.IsIndicator=true;
								Solver->LazyRows.push_back(NewRow);
								//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
								STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
								//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
							}
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
				if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){	
					for (int b=0; b<NumberOfBars; b++){		
						for (int i=0; i<NumberOfItems; i++){
							for (int p=0;p<P;p++){
								row NewRow;
	        					string Name=zBot[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i)
	        								+ "_"+std::to_string(p);
								NewRow.IndicatorVariable=Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str()));
								NewRow.complemented=false;	
								//out << zBot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p<<" = 1 -> "
								NewRow.Coef.push_back(1);
	        					Name=ABot[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								// << ABot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i <<" ";
								for (int stages=0;stages<t;stages++){
									for (int ii=0; ii<NumberOfItems; ii++){
										if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios[t-1][s], static_cast<std::size_t>(stages), ii)
											&& (t-ConsideredStages) <= stages){
											NewRow.Coef.push_back(-1);
	    									string Name=yBot[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p)+"_"+std::to_string((stages)*NumberOfItems+ii);
	   										NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
											//out <<"-"<< yBot[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<"_"<<(stages)*NumberOfItems+ii << " ";
										}
									}
								}
								NewRow.Coef.push_back(-1);
	        					Name=yBotCon[t-1].VarName+ "_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << "-" <<yBotCon[t-1].VarName<<"_"<<b<<"_"<<i<<"_"<<p<<
								// " <= 0"<<endl;
								NewRow.active=false;
								NewRow.sense='L';
								NewRow.rhs=0;
								NewRow.IsIndicator=true;
								Solver->LazyRows.push_back(NewRow);
								//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
								STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
								//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
								
							}
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
				if(STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].IsNew){
					for (int b=0; b<NumberOfBars; b++){		
						for (int i=0; i<NumberOfItems; i++){
							row NewRow;
							for (int p=0;p<P;p++){
								NewRow.Coef.push_back(1);
	        					string Name=zTop[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	       						NewRow.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << " +"<<zTop[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p;	
							}
							NewRow.active=false;
							NewRow.sense='E';
							NewRow.rhs=1;
							NewRow.IsIndicator=false;
							Solver->LazyRows.push_back(NewRow);
							//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
							STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
							//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
							//out << " =1"<<endl;

							row NewRow1;
							for (int p=0;p<P;p++){
								NewRow1.Coef.push_back(1);
	        					string Name=zBot[t-1].VarName+ "_"+STree.SubScenarios[t-1][s]+"_"+std::to_string(b)+ "_"+std::to_string(i)+ "_"+std::to_string(p);
	       						NewRow1.Index.push_back(Solver->GetVariableIndexByName(const_cast<char*>(Name.c_str())));
								//out << " +"<< zBot[t-1].VarName<<"_"<< STree.SubScenarios[t-1][s]<<"_"<< b << "_"<<  i << "_"<<  p;	
							}
							NewRow1.active=false;
							NewRow1.sense='E';
							NewRow1.rhs=1;
							NewRow1.IsIndicator=false;
							Solver->LazyRows.push_back(NewRow1);
							//std::cerr << "Level " << STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].level << " Node "<<STree.SubScenarios[t-1][s] << endl;
							STree.CurrentTree[STree.SubScenariosToNode[t-1][s]].RowIndices.push_back(Solver->LazyRows.size()-1);
							//std::cerr << "LazyRows Size=" <<Solver->LazyRows.size() << endl;
							//out << " =1"<<endl;
						}
					}
				}
			}
		}
	}


	//Pro Stufe genau ein Item
//	for (int t=1; t<NumberOfStages; t++){	
//		out <<"U_T"<<t<<": ";
//		for (int i=0; i<NumberOfItems; i++){		
//			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
//		}
//		out << "=1" <<endl;
//	}

	//Jedes Item genau A_i mal
//	for (int i=0; i<NumberOfItems; i++){	
//		out <<"U_I"<<i<<": ";
//		for (int t=1; t<NumberOfStages; t++){	
//			out << "+"<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
//		}
//		out << "= "<<Partitions[RandPart][i] <<endl;
//	}


	/*
	out<<"BOUNDS\n";
	AI.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out,true);	
	R.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out,true);	
	F.Print(out,true);
	NoR.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++){
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
	
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
	AI.Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		A[t].Print(out);
	//for (int t=0;t<NumberOfStages-1;t++)
	//	W[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		if(MoreThanOneP) zTop[t].Print(out);
	for (int t=0;t<NumberOfStages-1;t++)
		if(MoreThanOneP) zBot[t].Print(out);
	F.Print(out);
	NoR.Print(out);

	out<<"END\n";
	out.close();
*/

	return;
}

