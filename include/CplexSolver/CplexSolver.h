
#ifndef CPLEX_SOLVER_H_
#define CPLEX_SOLVER_H_
#include <vector>
#include <ilcplex/cplexx.h>
#include <string>
#include "DR_Holder.h"


class row{
public:
   bool active = false;
   double rhs = 0.0;
   char sense = 'L';
   std::vector<double> Coef;
   std::vector<int> Index;
   bool IsIndicator = false;
   int IndicatorVariable = -1;
   int complemented = 0;
};

class CplexSolver{
public:
   CplexSolver(char** argv, std::vector<int> SuccessorsInEachStage);
   double Solve(bool PrintOutput=false);
   double GetBestBound();
   int GetLastSolutionStatus() const;
   bool HasObjectiveValue() const;
   bool IsInfeasible() const;
   bool IsTimeLimit() const;
   void Clear();
   void ReadProblem(char* name);
   void SetTimeLimit(double seconds);
   void UpdateBound(double lowerBound);
   int GetVariableIndexByName(const std::string& variableName);
   void AddRow(const row& ThisRow);
   void AddAllNewRows();
   void PrintSolution();
   void ExtractDR(DR_Holder & DR);
   bool ExtractScenario(DR_Holder & DR);
   int getLastSolutionStatus() const { return solstat; }
   std::vector<row> LazyRows;
private:
	int      solstat;
	double   objval;
  
   int lastSolutionStatus = 0;
   bool lastObjectiveAvailable = false;
   double   *x;
   CPXENVptr     env;
   CPXLPptr      lp;
   int           status;
   CPXDIM        j;
   CPXDIM        cur_numcols;
   static void free_and_null (char **ptr);
   std::string statusToString(int statusCode) const;
};



#endif
