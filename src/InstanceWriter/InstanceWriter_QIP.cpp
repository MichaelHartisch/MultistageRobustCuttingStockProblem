#include "InstanceWriter.h"
#include "Variable.h"

using namespace std;

void InstanceWriter::writeQIP(){
	Variable AI("a",  3,  0,  1);
	AI.AddIndex(0,1);
	AI.AddIndex(1,NumberOfBars);
	AI.AddIndex(2,1);

	Variable A("a",  3,  0,  1);
	A.AddIndex(0,NumberOfStages,1);
	A.AddIndex(1,NumberOfBars);
	A.AddIndex(2,NumberOfItems);

	Variable F("f",  1,  0,  1);
    F.AddIndex(0,NumberOfBars,0);

	Variable W("w",  2,  0,  1);
	W.AddIndex(0,NumberOfStages,1);
	W.AddIndex(1,NumberOfBars);

	Variable R("r",  1,  0,  MinimumRest);
	R.AddIndex(0,NumberOfBars);

	Variable NoR("nr",  1,  0,  1);
    NoR.AddIndex(0,NumberOfBars);

	Variable Z("z",  2,  0,  1);
	Z.AddIndex(0,NumberOfStages,1);
	Z.AddIndex(1,NumberOfItems);



	stringstream mipout;
	mipout<<BaseName<<".qlp";
	ofstream out(mipout.str().c_str());
	out<<"MINIMIZE\n";
	//Minimize the number of queries
	for (int b=0; b<NumberOfBars; b++)			
		out<<"+ "<<CostPerUnit<<" "<<R.VarName<<"_"<<b<<" ";
	 for (int b=0; b<NumberOfBars; b++)                      
                out<<"+ "<<CostPerBar<<" "<<F.VarName<<"_"<<b<<" ";
	for (int t=1; t<NumberOfStages; t++)		
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+ "<<BarChangeCost<<" "<<W.VarName<<"_"<<t<<"_"<<b<<" ";


	out<<"\nSUBJECT TO\n";
	// Do not excced each bars/bins capacity
	//If bar is used set binary F variable
        for (int b=0; b<NumberOfBars; b++){             
                for (int t=0; t<NumberOfStages; t++){           
                        for (int i=0; i<NumberOfItems; i++){
                                out << "+ "<< A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
                        	if(t==0)break;
			}
                }
                out <<"- "<<NumberOfStages<<" "<<F.VarName<<"_"<<b<<" <= 0"<<endl;
        }
	
	for (int b=0; b<NumberOfBars; b++){		
		for (int t=1; t<NumberOfStages; t++){		
			for (int i=0; i<NumberOfItems; i++){
				out << "+ "<< LengthOfItems[i]<<" "<<A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
			}
		}
		out <<"+ " << LengthOfInitial << " "<<AI.VarName<<"_0_"<<b<<"_0 " << "+ "/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
	}

 for (int b=0; b<NumberOfBars; b++){             
                for (int t=1; t<NumberOfStages; t++){           
                        for (int i=0; i<NumberOfItems; i++){
                                out << "+ "<< LengthOfItems[i]<<" "<<A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
                        }
                }
                out <<"+ " << LengthOfInitial << " "<<AI.VarName<<"_0_"<<b<<"_0 " << "+ "/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" + " <<BarSizes[b]<<" "<<NoR.VarName<<"_"<<b<<" >= "<<BarSizes[b]<<endl;
        }

 for (int b=0; b<NumberOfBars; b++){             
                for (int t=1; t<NumberOfStages; t++){           
                        for (int i=0; i<NumberOfItems; i++){
                                out << "+ "<< LengthOfItems[i]<<" "<<A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
                        }
                }
                out <<"+ " << LengthOfInitial << " "<< AI.VarName<<"_0_"<<b<<"_0 " << " + "<<MinimumRest<<" "<<NoR.VarName<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
        }


	for (int b=0; b<NumberOfBars; b++){  
	out <<R.VarName<<"_"<<b <<" + " <<MinimumRest<<" "<<NoR.VarName<<"_"<<b<<" <= "<<MinimumRest<<endl;
	}

	// Ausgewähltes Item muss auch zugeordnet werden
	for (int t=1; t<NumberOfStages; t++){		
		for (int i=0; i<NumberOfItems; i++){
			for (int b=0; b<NumberOfBars; b++){		
				out << "+ "<< A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
			}
			out <<"- "<<Z.VarName<<"_"<<t<<"_"<<i<< " = 0"<<endl;
		}
	}
	//Initial Item muss zugewiesen werden
	for (int b=0; b<NumberOfBars; b++){		
		out << "+ "<< AI.VarName<<"_"<< 0<<"_"<< b << "_0 ";
	}
	out << " = 1"<<endl;

	// Wechselvariablen belegen


	for (int t=1; t<NumberOfStages; t++){		
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++){		
				out << "+ "<< A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
			}
			if(t==1){
				out << "- "<< AI.VarName<<"_0_"<< b << "_0 ";
			}
			else{
				for (int i=0; i<NumberOfItems; i++){		
					out << "- "<< A.VarName<<"_"<< t-1<<"_"<< b << "_"<<  i <<" ";
				}
			}
			out <<"- "<<W.VarName<<"_"<<t<<"_"<<b<< " <= 0 "<<endl;

/*			if(t==1){
				out << "+"<< AI.VarName<<"_0_"<< b << "_0 ";
			}
			else{
				for (int i=0; i<NumberOfItems; i++){		
					out << "+"<< A.VarName<<"_"<< t-1<<"_"<< b << "_"<<  i <<" ";
				}
			}
			for (int i=0; i<NumberOfItems; i++){		
				out << "-"<< A.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
			}
			out <<"-"<<W.VarName<<"_"<<t<<"_"<<b<< " <= 0 "<<endl;
*/
		}
	}


	//Pro Stufe genau ein Item
	out << "UNCERTAINTY SUBJECT TO" << endl;
	for (int t=1; t<NumberOfStages; t++){	
		for (int i=0; i<NumberOfItems; i++){		
			out << "+ "<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "= 1" <<endl;
	}

	//Jedes Item genau A_i mal
	for (int i=0; i<NumberOfItems; i++){	
		for (int t=1; t<NumberOfStages; t++){	
			out << "+ "<< Z.VarName<<"_"<< t<<"_"<<  i <<" ";
		}
		out << "= "<<Partitions[RandPart][i] <<endl;
	}

	out<<"BOUNDS\n";
	AI.Print(out,true);
	A.Print(out,true);
	R.Print(out,true);
	Z.Print(out,true);
	W.Print(out,true);
	F.Print(out,true);
	NoR.Print(out,true);
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
	AI.Print(out);
	A.Print(out);
	Z.Print(out);
	W.Print(out);
	F.Print(out);
	NoR.Print(out);
	//R.Print(out,false);
	out<<"EXISTS\n";
	F.Print(out);
	AI.Print(out);
	A.Print(out);
	R.Print(out);
	NoR.Print(out);
	W.Print(out);
	out<<"ALL\n";
	Z.Print(out);

	out<<"ORDER\n";
	AI.Print(out);

	for (int t=1;t<NumberOfStages;t++){
		Z.FixIndex(0,t);
		Z.Print(out);
		A.FixIndex(0,t);
		A.Print(out);
	//	W.FixIndex(0,t);
	//	W.Print(out, false);
	}
	W.Print(out);	
	R.Print(out);
	NoR.Print(out);
	 F.Print(out);

	
	out<<"END\n";
	out.close();


	return;
}

