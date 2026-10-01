#include "InstanceWriter.h"
#include "Variable.h"

using namespace std;

void InstanceWriter::writeRO(){

	Variable AMIP("a",  2,  0, NumberOfStages);
	AMIP.AddIndex(0,NumberOfBars);
    AMIP.AddIndex(1,NumberOfItems);

	Variable F("f",  1,  0,  1);
    F.AddIndex(0,NumberOfBars,0);

	Variable R("r",  1,  0,  MinimumRest);
	R.AddIndex(0,NumberOfBars);

	Variable NoR("nr",  1,  0,  1);
    NoR.AddIndex(0,NumberOfBars);

	Variable Z("z",  2,  0,  1);
	Z.AddIndex(0,NumberOfStages,1);
	Z.AddIndex(1,NumberOfItems);

	Variable U("u",  3,  0, 1);
	U.AddIndex(0,NumberOfStages); //No Offset this time
	U.AddIndex(1,NumberOfBars);
    U.AddIndex(2,NumberOfItems);

	Variable W("w",  2,  0,  1);
	W.AddIndex(0,NumberOfStages,1);
	W.AddIndex(1,NumberOfBars);




	stringstream mipout;
	mipout<<BaseName<<"_RO.qlp";
	ofstream out(mipout.str().c_str());
	out<<"MINIMIZE\n";
        //Minimize the number of queries
	for (int b=0; b<NumberOfBars; b++)                      
		out<<"+ "<<CostPerUnit<<R.VarName<<"_"<<b<<" ";
	for (int b=0; b<NumberOfBars; b++)                      
		out<<"+ "<<CostPerBar<<F.VarName<<"_"<<b<<" ";
	for (int t=1; t<NumberOfStages; t++)		
		for (int b=0; b<NumberOfBars; b++)			
			out<<"+ "<<BarChangeCost<<" "<<W.VarName<<"_"<<t<<"_"<<b<<" ";

	out<<"\nSUBJECT TO\n";
	//If bar is used set binary F variable
	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+ "<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out <<"- "<<NumberOfStages<<" "<<F.VarName<<"_"<<b<<" <= 0"<<endl;
	}
       	// Do not excced each bars/bins capacity

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+ "<< LengthOfItems[i]<<" "<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
                	out << "+ "/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" <= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+ "<< LengthOfItems[i]<<" "<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
                out << "+ "/*<<MinimumRest*/<<R.VarName<<"_"<<b <<" + " <<BarSizes[b]<<" "<<NoR.VarName<<"_"<<b<<" >= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){             
		for (int i=0; i<NumberOfItems; i++){
			out << "+ "<< LengthOfItems[i]<<" "<<AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out << " + "<<MinimumRest<<" "<<NoR.VarName<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
	}

	for (int b=0; b<NumberOfBars; b++){  
		out <<R.VarName<<"_"<<b <<" + " <<MinimumRest<<" "<<NoR.VarName<<"_"<<b<<" <= "<<MinimumRest<<endl;
	}

        // Ausgewähltes Item muss auch zugeordnet werden
	for (int i=0; i<NumberOfItems; i++){
		for (int b=0; b<NumberOfBars; b++){             
			out << "+ "<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
		}
		out << " = "<<Partitions[RandPart][i] +(i==IndexInitial)<<endl;
	}
	for (int t=1; t<NumberOfStages; t++){	
		for (int b=0; b<NumberOfBars; b++){
			for (int i=0; i<NumberOfItems; i++){
				out << "+ "<< AMIP.VarName<<"_"<< b << "_"<<  i <<" ";
				for(int tt=0;tt<=t;tt++){
					out << "- "<<U.VarName<<"_"<<tt<<"_"<<b<<"_"<<i<<" "; 
				}
				out <<">= 0"<<endl;
			}
		}
	}

	for (int b=0; b<NumberOfBars; b++)
		out << "+ "<< U.VarName<<"_"<< 0<<"_"<< b << "_"<<  IndexInitial <<" ";
	out <<"= "<<1<<endl;

	for (int t=1;t<NumberOfStages;t++){
		for(int i=0;i<NumberOfItems;i++){
			for (int b=0;b<NumberOfBars;b++){
				out << "+ "<<U.VarName<<"_"<<t<<"_"<<b<<"_"<<i<<" "; 
			}
			out <<"- "<<Z.VarName<<"_"<<t<<"_"<<i<<" = 0"<<endl;
		}
	}

	for (int t=1; t<NumberOfStages; t++){		
		for (int b=0; b<NumberOfBars; b++){		
			for (int i=0; i<NumberOfItems; i++)		
				out << "+ "<< U.VarName<<"_"<< t<<"_"<< b << "_"<<  i <<" ";
			for (int i=0; i<NumberOfItems; i++)	
				out << "- "<< U.VarName<<"_"<< t-1<<"_"<< b << "_"<<  i <<" ";
			out <<"- "<<W.VarName<<"_"<<t<<"_"<<b<< " <= 0 "<<endl;
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
	AMIP.Print(out,true);
 	R.Print(out,true);
    NoR.Print(out,true);
	F.Print(out,true);
	U.Print(out,true);
	W.Print(out,true);
	Z.Print(out,true);
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
    NoR.Print(out);
    F.Print(out);
    U.Print(out);
    W.Print(out);
	Z.Print(out);
	out<<"GENERALS\n";
	AMIP.Print(out);

	out<<"EXISTS\n";
	AMIP.Print(out);
	F.Print(out);
	R.Print(out);
	NoR.Print(out);
	W.Print(out);
	U.Print(out);
	out<<"ALL\n";
	Z.Print(out);

	out<<"ORDER\n";
	AMIP.Print(out);
	Z.Print(out);
	U.Print(out);
	F.Print(out);
	R.Print(out);
	NoR.Print(out);
	W.Print(out);
/*
	for (int t=1;t<NumberOfStages;t++){
		Z.FixIndex(0,t);
		Z.Print(out, false);
		A.FixIndex(0,t);
		A.Print(out, false);
	//	W.FixIndex(0,t);
	//	W.Print(out, false);
	}
	W.Print(out,false);	
	R.Print(out,false);
	NoR.Print(out,false);
	 F.Print(out,false);

	*/
	out<<"END\n";
	out.close();


	return;
}

