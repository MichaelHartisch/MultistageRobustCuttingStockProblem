#include "CplexSolver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <utility>

void CplexSolver::free_and_null(char** ptr)
{
   if (*ptr != NULL) {
      free(*ptr);
      *ptr = NULL;
   }
}

std::string CplexSolver::statusToString(int statusCode) const
{
   if (env != NULL) {
      char statusString[CPXMESSAGEBUFSIZE];
      if (CPXXgetstatstring(env, statusCode, statusString) != NULL) {
         return std::string(statusString);
      }
   }

   std::ostringstream oss;
   oss << "CPLEX status " << statusCode;
   return oss.str();
}

void CplexSolver::CheckStatus(int cplexStatus, const std::string& message) const
{
   if (cplexStatus == 0) {
      return;
   }

   std::ostringstream oss;
   oss << message << ". CPLEX error code " << cplexStatus;

   if (env != NULL) {
      char errmsg[CPXMESSAGEBUFSIZE];
      if (CPXXgeterrorstring(env, cplexStatus, errmsg) != NULL) {
         oss << ": " << errmsg;
      }
   }

   throw std::runtime_error(oss.str());
}

void CplexSolver::EnsureProblemExists(const std::string& context) const
{
   if (lp == NULL) {
      throw std::runtime_error(context + ": no CPLEX problem has been created");
   }
}

void CplexSolver::RememberVariable(const std::string& name, int index)
{
   if (!name.empty()) {
      variableIndexByName[name] = index;
   }
}

void CplexSolver::ClearVariableIndexCache()
{
   variableIndexByName.clear();
}

void CplexSolver::RebuildVariableIndexCache()
{
   ClearVariableIndexCache();

   if (lp == NULL) {
      return;
   }

   const CPXDIM ncols = CPXXgetnumcols(env, lp);
   if (ncols <= 0) {
      return;
   }

   char** colNames = NULL;
   char* colNameStore = NULL;
   CPXSIZE surplus = 0;

   int localStatus = CPXXgetcolname(env, lp, NULL, NULL, 0, &surplus, 0, ncols - 1);
   if (localStatus != CPXERR_NEGATIVE_SURPLUS && localStatus != 0) {
      CheckStatus(localStatus, "Could not determine space for CPLEX column names");
   }

   const CPXSIZE nameSpace = -surplus;
   if (nameSpace <= 0) {
      return;
   }

   colNames = static_cast<char**>(malloc(sizeof(*colNames) * ncols));
   colNameStore = static_cast<char*>(malloc(sizeof(*colNameStore) * nameSpace));

   if (colNames == NULL || colNameStore == NULL) {
      free(colNames);
      free(colNameStore);
      throw std::runtime_error("Failed to allocate memory for CPLEX column names");
   }

   localStatus = CPXXgetcolname(env, lp,
                                colNames,
                                colNameStore,
                                nameSpace,
                                &surplus,
                                0,
                                ncols - 1);
   if (localStatus) {
      free(colNames);
      free(colNameStore);
      CheckStatus(localStatus, "CPXXgetcolname failed");
   }

   for (CPXDIM i = 0; i < ncols; ++i) {
      if (colNames[i] != NULL) {
         variableIndexByName[std::string(colNames[i])] = static_cast<int>(i);
      }
   }

   free(colNames);
   free(colNameStore);
}

CplexSolver::CplexSolver(char** /*argv*/, std::vector<int> /*SuccessorsInEachStage*/)
{
   x = NULL;
   env = NULL;
   lp = NULL;
   status = 0;
   solstat = 0;
   objval = 0.0;
   lastObjectiveAvailable = false;

   env = CPXXopenCPLEX(&status);

   if (env == NULL) {
      std::ostringstream oss;
      oss << "Could not open CPLEX environment. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }

   const unsigned int NUM_THREADS = 1;
   status = CPXXsetintparam(env, CPX_PARAM_THREADS, NUM_THREADS);
   CheckStatus(status, "Failed to set CPLEX thread count");

   // The experiment-wide deadline is measured in wall-clock seconds.
   status = CPXXsetintparam(env, CPX_PARAM_CLOCKTYPE, 2);
   CheckStatus(status, "Failed to select CPLEX wall-clock time");

   status = CPXXsetintparam(env, CPXPARAM_ScreenOutput, CPX_OFF);
   CheckStatus(status, "Failure to turn off CPLEX screen output");
}

void CplexSolver::CreateProblem(const std::string& problemName)
{
   ClearModel();

   lp = CPXXcreateprob(env, &status, problemName.c_str());
   if (lp == NULL) {
      CheckStatus(status, "Failed to create CPLEX problem");
      throw std::runtime_error("Failed to create CPLEX problem");
   }

   SetMinimize();
   ClearVariableIndexCache();
   LazyRows.clear();
   hasStoredMipStart = false;
   storedMipStartAlreadyAdded = false;
   storedMipStartIndices.clear();
   storedMipStartValues.clear();
   lastObjectiveAvailable = false;
   solstat = 0;
   objval = 0.0;
}

void CplexSolver::ClearModel()
{
   free_and_null(reinterpret_cast<char**>(&x));

   if (lp != NULL) {
      status = CPXXfreeprob(env, &lp);
      CheckStatus(status, "CPXXfreeprob failed while clearing model");
      lp = NULL;
   }

   ClearVariableIndexCache();
   LazyRows.clear();
   hasStoredMipStart = false;
   storedMipStartAlreadyAdded = false;
   storedMipStartIndices.clear();
   storedMipStartValues.clear();
   lastObjectiveAvailable = false;
   solstat = 0;
   objval = 0.0;
}

bool CplexSolver::HasProblem() const
{
   return lp != NULL;
}

void CplexSolver::SetMinimize()
{
   EnsureProblemExists("SetMinimize");
   status = CPXXchgobjsen(env, lp, CPX_MIN);
   CheckStatus(status, "Failed to set objective sense to minimization");
}

void CplexSolver::SetMaximize()
{
   EnsureProblemExists("SetMaximize");
   status = CPXXchgobjsen(env, lp, CPX_MAX);
   CheckStatus(status, "Failed to set objective sense to maximization");
}

int CplexSolver::AddVariable(const std::string& name,
                             double lowerBound,
                             double upperBound,
                             char type,
                             double objectiveCoefficient)
{
   EnsureProblemExists("AddVariable");

   // Every column created by AddVariable is already recorded in this map,
   // and ReadProblem explicitly rebuilds it.  Avoid asking CPLEX to look up
   // a not-yet-existing column for every newly created variable.
   if (!name.empty() && variableIndexByName.find(name) != variableIndexByName.end()) {
      throw std::runtime_error("Variable already exists: " + name);
   }

   const CPXDIM newIndex = CPXXgetnumcols(env, lp);

   double obj = objectiveCoefficient;
   double lb = lowerBound;
   double ub = upperBound;
   char ctype = type;
   char* namePtr = const_cast<char*>(name.c_str());

   status = CPXXnewcols(env, lp, 1, &obj, &lb, &ub, &ctype, name.empty() ? NULL : &namePtr);
   CheckStatus(status, "Failed to add CPLEX variable '" + name + "'");

   RememberVariable(name, static_cast<int>(newIndex));
   storedMipStartAlreadyAdded = false;

   return static_cast<int>(newIndex);
}

int CplexSolver::AddContinuousVariable(const std::string& name,
                                       double lowerBound,
                                       double upperBound,
                                       double objectiveCoefficient)
{
   return AddVariable(name, lowerBound, upperBound, 'C', objectiveCoefficient);
}

int CplexSolver::AddBinaryVariable(const std::string& name,
                                   double objectiveCoefficient)
{
   return AddVariable(name, 0.0, 1.0, 'B', objectiveCoefficient);
}

int CplexSolver::AddIntegerVariable(const std::string& name,
                                    double lowerBound,
                                    double upperBound,
                                    double objectiveCoefficient)
{
   return AddVariable(name, lowerBound, upperBound, 'I', objectiveCoefficient);
}

int CplexSolver::FindVariableIndexByName(const std::string& variableName) const
{
   auto it = variableIndexByName.find(variableName);
   if (it != variableIndexByName.end()) {
      return it->second;
   }

   if (lp == NULL) {
      return -1;
   }

   CPXDIM colIndex = -1;
   const int lookupStatus = CPXXgetcolindex(env,
                                            lp,
                                            const_cast<char*>(variableName.c_str()),
                                            &colIndex);
   if (lookupStatus) {
      return -1;
   }

   return static_cast<int>(colIndex);
}

int CplexSolver::RequireVariableIndex(const std::string& variableName) const
{
   const int index = FindVariableIndexByName(variableName);
   if (index < 0) {
      throw std::runtime_error("Could not find CPLEX variable: " + variableName);
   }
   return index;
}

int CplexSolver::GetVariableIndexByName(const std::string& variableName)
{
   EnsureProblemExists("GetVariableIndexByName");

   const int existingIndex = FindVariableIndexByName(variableName);
   if (existingIndex >= 0) {
      RememberVariable(variableName, existingIndex);
      return existingIndex;
   }

   /*
    * Compatibility path for the existing incremental row builder.
    *
    * Older code sometimes prepares rows before all referenced variables have
    * been declared in the initial master. The legacy behavior was to create
    * such variables on demand. We keep that behavior for now so that the
    * internal refactor can be done gradually.
    *
    * New model builders should prefer explicit Add...Variable(...) calls.
    */
   double lb = 0.0;
   double ub = 1.0;
   char type = 'B';

   if (variableName.size() >= 2 &&
       variableName[0] != 'z' &&
       (variableName[1] == 'B' || variableName[1] == 'T')) {
      lb = -CPX_INFBOUND;
      ub = CPX_INFBOUND;
      type = 'C';
   }

   return AddVariable(variableName, lb, ub, type, 0.0);
}

void CplexSolver::ChangeLowerBound(int variableIndex, double lowerBound)
{
   EnsureProblemExists("ChangeLowerBound");
   CPXDIM idx = variableIndex;
   char lu = 'L';
   double bd = lowerBound;
   status = CPXXchgbds(env, lp, 1, &idx, &lu, &bd);
   CheckStatus(status, "Failed to change variable lower bound");
}

void CplexSolver::ChangeUpperBound(int variableIndex, double upperBound)
{
   EnsureProblemExists("ChangeUpperBound");
   CPXDIM idx = variableIndex;
   char lu = 'U';
   double bd = upperBound;
   status = CPXXchgbds(env, lp, 1, &idx, &lu, &bd);
   CheckStatus(status, "Failed to change variable upper bound");
}

void CplexSolver::ChangeBounds(int variableIndex, double lowerBound, double upperBound)
{
   EnsureProblemExists("ChangeBounds");

   CPXDIM indices[2] = {variableIndex, variableIndex};
   char lu[2] = {'L', 'U'};
   double bd[2] = {lowerBound, upperBound};

   status = CPXXchgbds(env, lp, 2, indices, lu, bd);
   CheckStatus(status, "Failed to change variable bounds");
}

void CplexSolver::SetObjectiveCoefficient(int variableIndex, double coefficient)
{
   EnsureProblemExists("SetObjectiveCoefficient");
   CPXDIM idx = variableIndex;
   double val = coefficient;
   status = CPXXchgobj(env, lp, 1, &idx, &val);
   CheckStatus(status, "Failed to change objective coefficient");
}

void CplexSolver::AddLinearConstraint(const std::string& name,
                                      const std::vector<int>& indices,
                                      const std::vector<double>& coefficients,
                                      char sense,
                                      double rhs)
{
   EnsureProblemExists("AddLinearConstraint");

   if (indices.size() != coefficients.size()) {
      throw std::runtime_error("Cannot add linear constraint '" + name + "': index/coefficient size mismatch");
   }

   CPXNNZ rmatbeg = 0;
   const CPXNNZ nzcnt = static_cast<CPXNNZ>(indices.size());
   char rowSense = sense;
   double rowRhs = rhs;
   char* rowName = const_cast<char*>(name.c_str());

   status = CPXXaddrows(env,
                        lp,
                        0,
                        1,
                        nzcnt,
                        &rowRhs,
                        &rowSense,
                        &rmatbeg,
                        nzcnt == 0 ? NULL : const_cast<int*>(indices.data()),
                        nzcnt == 0 ? NULL : const_cast<double*>(coefficients.data()),
                        NULL,
                        name.empty() ? NULL : &rowName);
   CheckStatus(status, "Failed to add linear constraint '" + name + "'");
   storedMipStartAlreadyAdded = false;
}

void CplexSolver::AddIndicatorConstraint(const std::string& name,
                                         int binaryVariableIndex,
                                         int complemented,
                                         const std::vector<int>& indices,
                                         const std::vector<double>& coefficients,
                                         char sense,
                                         double rhs)
{
   EnsureProblemExists("AddIndicatorConstraint");

   if (indices.size() != coefficients.size()) {
      throw std::runtime_error("Cannot add indicator constraint '" + name + "': index/coefficient size mismatch");
   }

   const CPXNNZ nzcnt = static_cast<CPXNNZ>(indices.size());
   char* rowName = const_cast<char*>(name.c_str());

   status = CPXXaddindconstr(env,
                             lp,
                             binaryVariableIndex,
                             complemented,
                             nzcnt,
                             rhs,
                             sense,
                             nzcnt == 0 ? NULL : const_cast<int*>(indices.data()),
                             nzcnt == 0 ? NULL : const_cast<double*>(coefficients.data()),
                             name.empty() ? NULL : rowName);
   CheckStatus(status, "Failed to add indicator constraint '" + name + "'");
   storedMipStartAlreadyAdded = false;
}

void CplexSolver::SetTimeLimit(double seconds)
{
   if (seconds > 0.0) {
      status = CPXXsetdblparam(env, CPX_PARAM_TILIM, seconds);
   } else {
      status = CPXXsetdblparam(env, CPX_PARAM_TILIM, CPX_INFBOUND);
   }
   CheckStatus(status, "Failed to set CPLEX time limit");
}

void CplexSolver::UpdateBound(double lowerBound)
{
   EnsureProblemExists("UpdateBound");
   const int objIndex = RequireVariableIndex("obj");
   ChangeLowerBound(objIndex, lowerBound);
}

void CplexSolver::AddRow(const row& thisRow)
{
   EnsureProblemExists("AddRow");

   if (thisRow.Coef.size() != thisRow.Index.size()) {
      throw std::runtime_error("Cannot add CPLEX row: coefficient/index size mismatch");
   }

   if (!thisRow.IsIndicator) {
      AddLinearConstraint("", thisRow.Index, thisRow.Coef, thisRow.sense, thisRow.rhs);
   } else {
      AddIndicatorConstraint("",
                             thisRow.IndicatorVariable,
                             thisRow.complemented,
                             thisRow.Index,
                             thisRow.Coef,
                             thisRow.sense,
                             thisRow.rhs);
   }
}

void CplexSolver::AddAllNewRows()
{
   for (const row& thisRow : LazyRows) {
      AddRow(thisRow);
   }
   LazyRows.clear();
}

bool CplexSolver::StoreCurrentSolutionAsMipStart()
{
   if (lp == NULL) {
      std::cerr << "Cannot store MIP start before creating a CPLEX problem\n";
      return false;
   }

   const CPXDIM ncols = CPXXgetnumcols(env, lp);
   if (ncols <= 0) {
      std::cerr << "Cannot store MIP start: model has no variables\n";
      return false;
   }

   std::vector<double> values(static_cast<std::size_t>(ncols), 0.0);
   status = CPXXgetx(env, lp, values.data(), 0, ncols - 1);
   if (status) {
      std::cerr << "Could not extract incumbent solution for MIP start. CPLEX error code "
                << status << "\n";
      return false;
   }

   storedMipStartValues = std::move(values);
   storedMipStartIndices.resize(static_cast<std::size_t>(ncols));
   std::iota(storedMipStartIndices.begin(), storedMipStartIndices.end(), 0);
   hasStoredMipStart = true;
   storedMipStartAlreadyAdded = false;

   std::cerr << "Stored MIP start with " << ncols << " variable values\n";
   return true;
}

bool CplexSolver::AddStoredMipStart()
{
   if (!hasStoredMipStart || storedMipStartValues.empty()) {
      return false;
   }
   if (storedMipStartAlreadyAdded) {
      return false;
   }
   if (lp == NULL) {
      std::cerr << "Cannot add MIP start before creating a CPLEX problem\n";
      return false;
   }
   if (storedMipStartIndices.size() != storedMipStartValues.size()) {
      std::cerr << "Cannot add MIP start: index/value size mismatch\n";
      return false;
   }

   const CPXNNZ nzcnt = static_cast<CPXNNZ>(storedMipStartValues.size());
   CPXNNZ beg = 0;
#ifdef CPX_MIPSTART_REPAIR
   int effort = CPX_MIPSTART_REPAIR;
#else
   int effort = CPX_MIPSTART_AUTO;
#endif

   status = CPXXaddmipstarts(env,
                             lp,
                             1,
                             nzcnt,
                             &beg,
                             storedMipStartIndices.data(),
                             storedMipStartValues.data(),
                             &effort,
                             NULL);
   if (status) {
      std::cerr << "Warning: failed to add MIP start. CPLEX error code "
                << status << "\n";
      return false;
   }

   storedMipStartAlreadyAdded = true;
   std::cerr << "Added MIP start with " << storedMipStartValues.size()
             << " variable values\n";
   return true;
}

void CplexSolver::ReadProblem(char* name)
{
   ClearModel();

   lp = CPXXcreateprob(env, &status, name);
   if (lp == NULL) {
      CheckStatus(status, "Failed to create CPLEX problem before reading LP file");
      throw std::runtime_error("Failed to create CPLEX problem before reading LP file");
   }

   status = CPXXreadcopyprob(env, lp, name, NULL);
   CheckStatus(status, std::string("Failed to read and copy problem data from ") + name);

   RebuildVariableIndexCache();
   lastObjectiveAvailable = false;
   solstat = 0;
   objval = 0.0;
}

void CplexSolver::ExtractDR(DR_Holder& DR)
{
   EnsureProblemExists("ExtractDR");

   cur_numcols = CPXXgetnumcols(env, lp);
   if (cur_numcols <= 0) {
      throw std::runtime_error("Cannot extract decision rule: model has no columns");
   }

   free_and_null(reinterpret_cast<char**>(&x));
   x = static_cast<double*>(malloc(cur_numcols * sizeof(*x)));

   if (x == NULL) {
      throw std::runtime_error("No memory for CPLEX solution values");
   }

   status = CPXXgetx(env, lp, x, 0, cur_numcols - 1);
   CheckStatus(status, "Failed to obtain solution for decision-rule extraction");

   char** cur_colname = NULL;
   char* cur_colnamestore = NULL;
   CPXSIZE cur_colnamespace = 0;
   CPXSIZE surplus = 0;

   status = CPXXgetcolname(env, lp, NULL, NULL, 0, &surplus, 0, cur_numcols - 1);
   if (status != CPXERR_NEGATIVE_SURPLUS && status != 0) {
      CheckStatus(status, "Could not determine amount of space for column names");
   }

   cur_colnamespace = -surplus;
   if (cur_colnamespace > 0) {
      cur_colname = static_cast<char**>(malloc(sizeof(*cur_colname) * cur_numcols));
      cur_colnamestore = static_cast<char*>(malloc(sizeof(*cur_colnamestore) * cur_colnamespace));
      if (cur_colname == NULL || cur_colnamestore == NULL) {
         free(cur_colname);
         free(cur_colnamestore);
         throw std::runtime_error("Failed to allocate memory for column names");
      }

      status = CPXXgetcolname(env,
                              lp,
                              cur_colname,
                              cur_colnamestore,
                              cur_colnamespace,
                              &surplus,
                              0,
                              cur_numcols - 1);
      if (status) {
         free(cur_colname);
         free(cur_colnamestore);
         CheckStatus(status, "CPXXgetcolname failed");
      }
   }

   for (j = 0; j < cur_numcols; j++) {
      if (cur_colnamespace > 0 &&
          cur_colname[j] != NULL &&
          cur_colname[j][0] == 'y' &&
          (cur_colname[j][1] == 'T' || cur_colname[j][1] == 'B')) {
         if (cur_colname[j][2] == '_') {
            DR.set(std::string(cur_colname[j]), x[j]);
         } else {
            DR.setConst(std::string(cur_colname[j]), x[j]);
         }
      } else if (cur_colnamespace > 0 &&
                 cur_colname[j] != NULL &&
                 cur_colname[j][0] == 'a' &&
                 cur_colname[j][1] == 'i' &&
                 std::fabs(x[j] - 1.0) <= DR_Holder::BinaryTolerance) {
         DR.SetFirstStageBar(std::string(cur_colname[j]));
      }
   }

   free(cur_colname);
   free(cur_colnamestore);

   DR.SetOptimalValue(objval);
}

bool CplexSolver::ExtractScenario(DR_Holder& DR)
{
   EnsureProblemExists("ExtractScenario");

   cur_numcols = CPXXgetnumcols(env, lp);
   if (cur_numcols <= 0) {
      return false;
   }

   free_and_null(reinterpret_cast<char**>(&x));
   x = static_cast<double*>(malloc(cur_numcols * sizeof(*x)));

   if (x == NULL) {
      throw std::runtime_error("No memory for CPLEX solution values");
   }

   status = CPXXgetx(env, lp, x, 0, cur_numcols - 1);
   if (status) {
      std::cerr << "Failed to obtain solution for scenario extraction. CPLEX error code "
                << status << "\n";
      return false;
   }

   char** cur_colname = NULL;
   char* cur_colnamestore = NULL;
   CPXSIZE cur_colnamespace = 0;
   CPXSIZE surplus = 0;

   status = CPXXgetcolname(env, lp, NULL, NULL, 0, &surplus, 0, cur_numcols - 1);
   if (status != CPXERR_NEGATIVE_SURPLUS && status != 0) {
      CheckStatus(status, "Could not determine amount of space for column names");
   }

   cur_colnamespace = -surplus;
   if (cur_colnamespace > 0) {
      cur_colname = static_cast<char**>(malloc(sizeof(*cur_colname) * cur_numcols));
      cur_colnamestore = static_cast<char*>(malloc(sizeof(*cur_colnamestore) * cur_colnamespace));
      if (cur_colname == NULL || cur_colnamestore == NULL) {
         free(cur_colname);
         free(cur_colnamestore);
         throw std::runtime_error("Failed to allocate memory for column names");
      }

      status = CPXXgetcolname(env,
                              lp,
                              cur_colname,
                              cur_colnamestore,
                              cur_colnamespace,
                              &surplus,
                              0,
                              cur_numcols - 1);
      if (status) {
         free(cur_colname);
         free(cur_colnamestore);
         CheckStatus(status, "CPXXgetcolname failed");
      }
   }

   DR.PrepareForScenarioCollection();
   for (j = 0; j < cur_numcols; j++) {
      if (cur_colnamespace > 0 &&
          cur_colname[j] != NULL &&
          cur_colname[j][0] == 'z' &&
          cur_colname[j][1] == '_') {
         DR.CollectScenario(std::string(cur_colname[j]), x[j]);
      }
   }

   free(cur_colname);
   free(cur_colnamestore);

   return DR.BuildAndAddScenarioString();
}

void CplexSolver::PrintSolution()
{
   EnsureProblemExists("PrintSolution");

   cur_numcols = CPXXgetnumcols(env, lp);

   free_and_null(reinterpret_cast<char**>(&x));
   x = static_cast<double*>(malloc(cur_numcols * sizeof(*x)));

   if (x == NULL) {
      throw std::runtime_error("No memory for CPLEX solution values");
   }

   status = CPXXgetx(env, lp, x, 0, cur_numcols - 1);
   if (status) {
      std::cerr << "Failed to obtain solution. CPLEX error code " << status << "\n";
      return;
   }

   char** cur_colname = NULL;
   char* cur_colnamestore = NULL;
   CPXSIZE cur_colnamespace = 0;
   CPXSIZE surplus = 0;

   status = CPXXgetcolname(env, lp, NULL, NULL, 0, &surplus, 0, cur_numcols - 1);
   if (status != CPXERR_NEGATIVE_SURPLUS && status != 0) {
      CheckStatus(status, "Could not determine amount of space for column names");
   }

   cur_colnamespace = -surplus;
   if (cur_colnamespace > 0) {
      cur_colname = static_cast<char**>(malloc(sizeof(*cur_colname) * cur_numcols));
      cur_colnamestore = static_cast<char*>(malloc(sizeof(*cur_colnamestore) * cur_colnamespace));
      if (cur_colname == NULL || cur_colnamestore == NULL) {
         free(cur_colname);
         free(cur_colnamestore);
         throw std::runtime_error("Failed to allocate memory for column names");
      }

      status = CPXXgetcolname(env,
                              lp,
                              cur_colname,
                              cur_colnamestore,
                              cur_colnamespace,
                              &surplus,
                              0,
                              cur_numcols - 1);
      if (status) {
         free(cur_colname);
         free(cur_colnamestore);
         CheckStatus(status, "CPXXgetcolname failed");
      }
   }

   for (j = 0; j < cur_numcols; j++) {
      if (cur_colnamespace > 0 && cur_colname[j] != NULL) {
         std::cout << cur_colname[j] << ": " << x[j] << "\n";
      } else {
         std::cout << "x" << j << ": " << x[j] << "\n";
      }
   }

   free(cur_colname);
   free(cur_colnamestore);
}

double CplexSolver::Solve(bool PrintOutput)
{
   EnsureProblemExists("Solve");

   lastObjectiveAvailable = false;
   solstat = 0;
   objval = std::numeric_limits<double>::quiet_NaN();

   status = CPXXsetintparam(env, CPXPARAM_ScreenOutput, PrintOutput ? CPX_ON : CPX_OFF);
   CheckStatus(status, "Failed to set CPLEX screen output parameter");

   status = CPXXmipopt(env, lp);
   CheckStatus(status, "Failed to optimize MIP");

   solstat = CPXXgetstat(env, lp);
   //std::cerr << "CPLEX solution status " << solstat << ": "  << statusToString(solstat) << std::endl;

   status = CPXXgetobjval(env, lp, &objval);

   if (status) {
      if (IsInfeasible() || IsTimeLimit()) {
         if (PrintOutput) {
            std::cerr << "No objective value available for CPLEX status "
                      << solstat << " (" << statusToString(solstat)
                      << "), CPLEX error code " << status << ".\n";
         }
         lastObjectiveAvailable = false;
         return std::numeric_limits<double>::quiet_NaN();
      }

      std::ostringstream oss;
      oss << "Failed to obtain objective value. Solver status " << solstat
          << " (" << statusToString(solstat) << "), CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }

   lastObjectiveAvailable = true;
   return objval;
}

double CplexSolver::GetBestBound()
{
   EnsureProblemExists("GetBestBound");

   double bestBound = 0.0;
   status = CPXXgetbestobjval(env, lp, &bestBound);
   CheckStatus(status, "Failed to obtain CPLEX best bound");
   return bestBound;
}

int CplexSolver::GetLastSolutionStatus() const
{
   return solstat;
}

bool CplexSolver::HasObjectiveValue() const
{
   return lastObjectiveAvailable;
}

bool CplexSolver::IsInfeasible() const
{
#ifdef CPXMIP_INFEASIBLE
   if (solstat == CPXMIP_INFEASIBLE) {
      return true;
   }
#endif
   return solstat == 103;
}

bool CplexSolver::IsTimeLimit() const
{
#ifdef CPXMIP_TIME_LIM_FEAS
   if (solstat == CPXMIP_TIME_LIM_FEAS) {
      return true;
   }
#endif
#ifdef CPXMIP_TIME_LIM_INFEAS
   if (solstat == CPXMIP_TIME_LIM_INFEAS) {
      return true;
   }
#endif
   return solstat == 107 || solstat == 108;
}

bool CplexSolver::IsOptimal() const
{
   // Both statuses certify MIP optimality up to CPLEX's configured tolerances.
   return solstat == CPXMIP_OPTIMAL || solstat == CPXMIP_OPTIMAL_TOL;
}

void CplexSolver::Clear()
{
   ClearModel();

   if (env != NULL) {
      status = CPXXcloseCPLEX(&env);
      if (status) {
         char errmsg[CPXMESSAGEBUFSIZE];
         std::cerr << "Could not close CPLEX environment. Error code " << status << "\n";
         if (CPXXgeterrorstring(env, status, errmsg) != NULL) {
            std::cerr << errmsg << "\n";
         }
      }
      env = NULL;
   }

   std::cerr << "Done with clear" << std::endl;
}
