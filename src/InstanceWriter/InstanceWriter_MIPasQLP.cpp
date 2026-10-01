#include "InstanceWriter.h"
#include "Variable.h"
using namespace std;

void InstanceWriter::writeMIPasQLP(){
	

	Variable AMIP("a",  2,  0, NumberOfStages);
	AMIP.AddIndex(0,NumberOfBars);
    AMIP.AddIndex(1,NumberOfItems);

	Variable F("f",  1,  0,  1);
    F.AddIndex(0,NumberOfBars,0);


	Variable R("r",  1,  0,  MinimumRest);
	R.AddIndex(0,NumberOfBars);

	Variable NoR("nr",  1,  0,  1);
    NoR.AddIndex(0,NumberOfBars);


	stringstream mipout;
	mipout<<BaseName<<"_MIP.qlp";
	ofstream out(mipout.str().c_str());

	out<<"MINIMIZE\n";
        //Minimize the number of queries
	for (int b=0; b<NumberOfBars; b++)                      
		out<<"+ "<<CostPerUnit<<" "<<R.VarName<<"_"<<b<<" ";
	for (int b=0; b<NumberOfBars; b++)                      
		out<<"+ "<<(CostPerBar+BarChangeCost)<<" "<<F.VarName<<"_"<<b<<" ";
	out <<"- " <<BarChangeCost;

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
		out << "+ "<<MinimumRest<<" "<<NoR.VarName<<"_"<<b<<" <= "<<BarSizes[b]<<endl;
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
	out<<"BOUNDS\n";
	AMIP.Print(out,true);
 	R.Print(out,true);
    NoR.Print(out,true);
	F.Print(out,true);
	//PrintBinaryBounds(out,"q",NumberOfItems,NumberOfItems);
	//PrintBinaryBounds(out,"x",NumberOfItems,NumberOfItems);

	out<<"BINARIES\n";
    NoR.Print(out,false);
    F.Print(out,false);

	out<<"GENERALS\n";
	AMIP.Print(out,false);

	out<<"EXISTS\n";
	AMIP.Print(out);
 	R.Print(out);
    NoR.Print(out);
	F.Print(out);
	out<<"ALL\n";
	out<<"ORDER\n";
	AMIP.Print(out);
 	R.Print(out);
    NoR.Print(out);
	F.Print(out);
	out<<"END\n";
	out.close();

	return;
}

