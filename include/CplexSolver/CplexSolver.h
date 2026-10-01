#ifndef CPLEX_SOLVER_H_
#define CPLEX_SOLVER_H_

#include "DR_Holder.h"

#include <ilcplex/cplexx.h>

#include <string>
#include <unordered_map>
#include <vector>

/*
 * Lightweight representation of a row that is prepared outside CplexSolver and
 * added later. This is kept for the incremental DR master logic.
 */
class row {
public:
   bool active = false;
   double rhs = 0.0;
   char sense = 'L'; // 'L', 'G', or 'E'

   std::vector<double> Coef;
   std::vector<int> Index;

   bool IsIndicator = false;
   int IndicatorVariable = -1;
   int complemented = 0; // 0: indicator variable == 1, 1: indicator variable == 0
};

/*
 * Thin wrapper around the CPLEX C API.
 *
 * The old workflow used LP files:
 *     write model to disk -> ReadProblem(...)
 *
 * The new workflow should build models directly through this class:
 *     CreateProblem(...)
 *     AddContinuousVariable(...), AddBinaryVariable(...)
 *     AddLinearConstraint(...), AddIndicatorConstraint(...)
 *     Solve(...)
 *
 * The old ReadProblem(...) method is intentionally kept for debugging and for
 * non-migrated code paths, but the decision-rule workflow should no longer
 * depend on it.
 */
class CplexSolver {
public:
   CplexSolver(char** argv, std::vector<int> SuccessorsInEachStage);

   /*
    * Model lifecycle.
    */
   void CreateProblem(const std::string& problemName);
   void ClearModel();
   bool HasProblem() const;

   /*
    * Objective sense.
    */
   void SetMinimize();
   void SetMaximize();

   /*
    * Variable creation.
    *
    * Return value is the CPLEX column index of the created variable.
    * Bounds may use +/- CPX_INFBOUND.
    */
   int AddVariable(const std::string& name,
                   double lowerBound,
                   double upperBound,
                   char type,
                   double objectiveCoefficient = 0.0);

   int AddContinuousVariable(const std::string& name,
                             double lowerBound,
                             double upperBound,
                             double objectiveCoefficient = 0.0);

   int AddBinaryVariable(const std::string& name,
                         double objectiveCoefficient = 0.0);

   int AddIntegerVariable(const std::string& name,
                          double lowerBound,
                          double upperBound,
                          double objectiveCoefficient = 0.0);

   /*
    * Variable lookup.
    *
    * FindVariableIndexByName returns -1 if the variable does not exist.
    * RequireVariableIndex throws if the variable does not exist.
    *
    * GetVariableIndexByName is kept for compatibility with the existing
    * incremental-row code. In the new internal builders, prefer explicit
    * Add...Variable(...) plus RequireVariableIndex(...).
    */
   int FindVariableIndexByName(const std::string& variableName) const;
   int RequireVariableIndex(const std::string& variableName) const;
   int GetVariableIndexByName(const std::string& variableName);

   /*
    * Bounds and objective coefficients.
    */
   void ChangeLowerBound(int variableIndex, double lowerBound);
   void ChangeUpperBound(int variableIndex, double upperBound);
   void ChangeBounds(int variableIndex, double lowerBound, double upperBound);
   void SetObjectiveCoefficient(int variableIndex, double coefficient);

   /*
    * Linear and indicator constraints.
    *
    * sense is one of:
    *   'L'  <=
    *   'G'  >=
    *   'E'  =
    */
   void AddLinearConstraint(const std::string& name,
                            const std::vector<int>& indices,
                            const std::vector<double>& coefficients,
                            char sense,
                            double rhs);

   void AddIndicatorConstraint(const std::string& name,
                               int binaryVariableIndex,
                               int complemented,
                               const std::vector<int>& indices,
                               const std::vector<double>& coefficients,
                               char sense,
                               double rhs);

   /*
    * Existing lazy-row interface used by the incremental DR master.
    */
   void AddRow(const row& ThisRow);
   void AddAllNewRows();
   std::vector<row> LazyRows;

   /*
    * Solving and solution status.
    */
   double Solve(bool PrintOutput = false);
   void SetTimeLimit(double seconds);
   void UpdateBound(double lowerBound);
   double GetBestBound();

   int GetLastSolutionStatus() const;
   int getLastSolutionStatus() const { return GetLastSolutionStatus(); }

   bool HasObjectiveValue() const;
   bool IsInfeasible() const;
   bool IsTimeLimit() const;

   /*
    * Legacy/debug file-based input. Should not be used by the final internal
    * decision-rule workflow.
    */
   void ReadProblem(char* name);

   /*
    * Warm start support for the incremental master.
    */
   bool StoreCurrentSolutionAsMipStart();
   bool AddStoredMipStart();

   /*
    * Solution extraction helpers used by the decision-rule algorithm.
    */
   void PrintSolution();
   void ExtractDR(DR_Holder& DR);
   bool ExtractScenario(DR_Holder& DR);

   /*
    * Release the CPLEX problem and environment.
    */
   void Clear();

private:
   int solstat = 0;
   double objval = 0.0;
   bool lastObjectiveAvailable = false;

   double* x = nullptr;

   CPXENVptr env = nullptr;
   CPXLPptr lp = nullptr;

   int status = 0;
   CPXDIM j = 0;
   CPXDIM cur_numcols = 0;

   std::unordered_map<std::string, int> variableIndexByName;

   std::vector<CPXDIM> storedMipStartIndices;
   std::vector<double> storedMipStartValues;
   bool hasStoredMipStart = false;
   bool storedMipStartAlreadyAdded = false;

   static void free_and_null(char** ptr);

   void EnsureProblemExists(const std::string& context) const;
   void CheckStatus(int cplexStatus, const std::string& message) const;

   void RememberVariable(const std::string& name, int index);
   void ClearVariableIndexCache();
   void RebuildVariableIndexCache();

   std::string statusToString(int statusCode) const;
};

#endif
