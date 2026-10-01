#include "InstanceWriter.h"
#include "Variable.h"

using namespace std;

void InstanceWriter::writeRO_RC(){
	STree.BuildEntireTree(getPartition());
	Variable AMIP("a",  2,  0, NumberOfStages);
	AMIP.AddIndex(0,NumberOfBars);
    AMIP.AddIndex(1,NumberOfItems);

	Variable Z("z",  2,  0,  1);
	Z.AddIndex(0,NumberOfStages,1);
	Z.AddIndex(1,NumberOfItems);

    vector <Variable> U;
	for (int t=0;t<NumberOfStages;t++){
		Variable Uv("u_"+std::to_string(t),  3,  0, 1);
		Uv.AddIndex(0,STree.SubScenarios.back().size());
		Uv.AddIndex(1,NumberOfBars);
    	Uv.AddIndex(2,NumberOfItems);
    U.push_back(Uv);
	}

	Variable UIni("u_i",  2,  0, 1);
	UIni.AddIndex(0,NumberOfBars);
	UIni.AddIndex(1,NumberOfItems);


	Variable F("f", 1 ,  0,  1);
    F.AddIndex(0,NumberOfBars);

    vector <Variable> W;
	for (int t=0;t<NumberOfStages;t++){
		Variable Wv("w_"+std::to_string(t),  2,  0,  1);
		Wv.AddIndex(0,STree.SubScenarios.back().size());
		Wv.AddIndex(1,NumberOfBars);
		W.push_back(Wv);
	}

	Variable R("r",  1,  0,  MinimumRest);
    R.AddIndex(0,NumberOfBars);

    Variable NoR("nr",  1,  0,  1);
    NoR.AddIndex(0,NumberOfBars);





	stringstream mipout;
	mipout<<BaseName<<"_RO.lp";
	ofstream out(mipout.str().c_str());
	out<<"MINIMIZE\n";
	out << "obj\n";

	out<<"\nSUBJECT TO\n";
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+"<<CostPerUnit<<R.VarName<<"_"<<b<<" ";
		 for (int b=0; b<NumberOfBars; b++)                      
	                out<<"+"<<CostPerBar<<F.VarName<<"_"<<b<<" ";
		for (int t=1; t<NumberOfStages; t++)		
			for (int b=0; b<NumberOfBars; b++)			
				out<<"+"<<BarChangeCost<<W[t-1].VarName<<"_"<<s<<"_"<<b<<" ";
		out <<"-obj <= 0\n";
	}
	//If bar is used set binary F variable
	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+"<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out <<"-"<<NumberOfStages<<F.VarName<<"_"<<b<<" <= 0"<<endl;
	}
       	// Do not excced each bars/bins capacity

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+"<< LengthOfItems[i]<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
                	out << "+"/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+"<< LengthOfItems[i]<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
                out << "+"/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" +" <<BarSizes[b]<<NoR.VarName<<"_"<<b<<" >= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+"<< LengthOfItems[i]<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out << " +"<<MinimumRest<<NoR.VarName<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){  
		out <<R.VarName<<"_"<<b <<" +" <<MinimumRest<<NoR.VarName<<"_"<<b<<" <= "<<MinimumRest<<endl;
	}

        // Ausgewähltes Item muss auch zugeordnet werden
	for (int i=0; i<NumberOfItems; i++){
		for (int b=0; b<NumberOfBars; b++){             
			out << "+"<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out << " = "<<Partitions[RandPart][i] +(i==IndexInitial)<<endl;
	}
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){	
		for (int b=0; b<NumberOfBars; b++){
			for (int i=0; i<NumberOfItems; i++){
				out << "+"<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
				for(int t=1;t<NumberOfStages;t++){
					out << "-"<<U[t-1].VarName<<"_"<<  s<<"_"<<b<<"_"<<i<<" "; 
				}
				if(i==IndexInitial) out << "-"<< UIni.VarName<<"_"<< b << "_"<<  IndexInitial <<" ";
				out <<">= 0"<<endl;
			}
		}
	}

	for (int b=0; b<NumberOfBars; b++)
		out << "+"<< UIni.VarName<<"_"<< b << "_"<<  IndexInitial <<" ";
	out <<"="<<1<<endl;

	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){	
		for (int t=1;t<NumberOfStages;t++){
			for(int i=0;i<NumberOfItems;i++){
				for (int b=0;b<NumberOfBars;b++){
					out << "+"<<U[t-1].VarName<<"_"<<  s<<"_"<<b<<"_"<<i<<" "; 
				}
				//out <<"-"<<Z.VarName<<"_"<<t<<"_"<<i<<" = 0"<<endl;
				if(ScenarioEncoding::scenarioItemEquals(STree.SubScenarios.back()[s], static_cast<std::size_t>(t-1), i)) out <<" = 1" <<endl;
				else out <<" = 0" <<endl; 
			}
		}
	}
	for (std::size_t s = 0; s < STree.SubScenarios.back().size(); ++s){	
		for (int t=1; t<NumberOfStages; t++){		
			for (int b=0; b<NumberOfBars; b++){		
				for (int i=0; i<NumberOfItems; i++)		
					out << "+"<< U[t-1].VarName<<"_"<<  s<<"_"<< b << "_"<<  i <<" ";
				if(t>1){
					for (int i=0; i<NumberOfItems; i++)	
						out << "-"<< U[t-2].VarName<<"_"<<  s<<"_"<< b << "_"<<  i <<" ";
				}
				else out << "-"<< UIni.VarName<<"_"<< b << "_"<<  IndexInitial <<" ";
				out <<"-"<<W[t-1].VarName<<"_"<< s<<"_"<<b<< " <= 0 "<<endl;
			}
		}
	}

		//Pro Stufe genau ein Item

	out<<"BOUNDS\n";
	AMIP.Print(out,true);
 	R.Print(out,true);
    NoR.Print(out,true);
	F.Print(out,true);
	for (int t=0;t<NumberOfStages-1;t++)
		U[t].Print(out,true);	
	for (int t=0;t<NumberOfStages-1;t++)
		W[t].Print(out,true);	
	//Z.Print(out,true);
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
    NoR.Print(out);
    F.Print(out);
    for (int t=0;t<NumberOfStages-1;t++)
		U[t].Print(out);	
	//for (int t=0;t<NumberOfStages-1;t++)
	//	W[t].Print(out);	
	out<<"GENERALS\n";
	AMIP.Print(out);


	out<<"END\n";
	out.close();


	return;
}

