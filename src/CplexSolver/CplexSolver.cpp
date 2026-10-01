
#include "CplexSolver.h"

#include <limits>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
void CplexSolver::free_and_null (char **ptr)
{
   if ( *ptr != NULL ) {
      free (*ptr);
      *ptr = NULL;
   }
} /* END free_and_null */ 

std::string CplexSolver::statusToString(int statusCode) const {
   char statusString[CPXMESSAGEBUFSIZE];
   if (env != NULL && CPXXgetstatstring(env, statusCode, statusString) != NULL) {
      return std::string(statusString);
   }
   std::ostringstream oss;
   oss << "CPLEX status " << statusCode;
   return oss.str();
}

CplexSolver::CplexSolver (char** /*argv*/, std::vector<int> /*SuccessorsInEachStage*/){
   /* Declare and allocate space for the variables and arrays where we will
      store the optimization results including the status, objective value,
      and variable values. */

   x = NULL;
   env = NULL;
   lp = NULL;

   /* Initialize the CPLEX environment */
   env = CPXXopenCPLEX (&status);

   /* If an error occurs, the status value indicates the reason for
      failure.  A call to CPXXgeterrorstring will produce the text of
      the error message.  Note that CPXXopenCPLEX produces no output,
      so the only way to see the cause of the error is to use
      CPXXgeterrorstring.  For other CPLEX routines, the errors will
      be seen if the CPXPARAM_ScreenOutput indicator is set to CPX_ON.  */

   if ( env == NULL ) {
      char  errmsg[CPXMESSAGEBUFSIZE];
      fprintf (stderr, "Could not open CPLEX environment.\n");
      CPXXgeterrorstring (env, status, errmsg);
      fprintf (stderr, "%s", errmsg);
      exit (EXIT_FAILURE);
   }
   const unsigned int NUM_THREADS = 1;
   CPXXsetintparam(env, CPX_PARAM_THREADS, NUM_THREADS);

   /* Turn on output to the screen */

   status = CPXXsetintparam (env, CPXPARAM_ScreenOutput, CPX_OFF);
   if ( status ) {
      fprintf (stderr, 
               "Failure to turn on screen indicator, error %d.\n", status);
      exit (EXIT_FAILURE);
   }
   /* Create the problem, using the filename as the problem name */
} 

int CplexSolver::GetLastSolutionStatus() const
{
    return lastSolutionStatus;
}

bool CplexSolver::HasObjectiveValue() const
{
    return lastObjectiveAvailable;
}

bool CplexSolver::IsInfeasible() const
{
    // 103 = CPXMIP_INFEASIBLE, "integer infeasible"
    return lastSolutionStatus == 103;
}

bool CplexSolver::IsTimeLimit() const
{
    // 107 = time limit exceeded with incumbent
    // 108 = time limit exceeded without incumbent
    return lastSolutionStatus == 107 || lastSolutionStatus == 108;
}

double CplexSolver::GetBestBound()
{
   double bestBound = 0.0;

   int status = CPXXgetbestobjval(env, lp, &bestBound);

   if (status) {
      std::ostringstream oss;
      oss << "Failed to obtain CPLEX best bound. CPLEX error code "
          << status;
      throw std::runtime_error(oss.str());
   }

   return bestBound;
}

void CplexSolver::SetTimeLimit(double seconds){
   if (seconds > 0.0) {
      status = CPXXsetdblparam(env, CPX_PARAM_TILIM, seconds);
   } else {
      status = CPXXsetdblparam(env, CPX_PARAM_TILIM, CPX_INFBOUND);
   }
   if (status) {
      std::ostringstream oss;
      oss << "Failed to set CPLEX time limit. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
}

void CplexSolver::UpdateBound(double lowerBound){
   if (lp == NULL) {
      throw std::runtime_error("Cannot update bound before reading a CPLEX problem");
   }
   const std::string name = "obj";
   CPXDIM colindex = -1;
   status = CPXXgetcolindex(env, lp, const_cast<char*>(name.c_str()), &colindex);
   if (status) {
      std::ostringstream oss;
      oss << "Could not find objective-bound variable 'obj'. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
   char lu = 'L';
   status = CPXXchgbds(env, lp, 1, &colindex, &lu, &lowerBound);
   if (status) {
      std::ostringstream oss;
      oss << "Failed to update objective lower bound. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
}

int CplexSolver::GetVariableIndexByName(const std::string& variableName){
   if (lp == NULL) {
      throw std::runtime_error("Cannot look up CPLEX variable before reading a problem");
   }
   CPXDIM colindex = -1;
   status = CPXXgetcolindex(env, lp, const_cast<char*>(variableName.c_str()), &colindex);
   if (!status) {
      return static_cast<int>(colindex);
   }

   double lb = 0.0;
   double ub = 1.0;
   char type = 'B';
   if (variableName.size() >= 2 && variableName[0] != 'z' &&
       (variableName[1] == 'B' || variableName[1] == 'T')) {
      lb = -CPX_INFBOUND;
      ub = CPX_INFBOUND;
      type = 'C';
   }

   char* namePtr = const_cast<char*>(variableName.c_str());
   status = CPXXaddcols(env, lp, 1, 0, NULL, NULL, NULL, NULL, &lb, &ub, &namePtr);
   if (status) {
      std::ostringstream oss;
      oss << "Failed to add missing variable '" << variableName << "'. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
   status = CPXXgetcolindex(env, lp, const_cast<char*>(variableName.c_str()), &colindex);
   if (status) {
      std::ostringstream oss;
      oss << "Could not find newly added variable '" << variableName << "'. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
   status = CPXXchgctype(env, lp, 1, &colindex, &type);
   if (status) {
      std::ostringstream oss;
      oss << "Failed to set type of variable '" << variableName << "'. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }
   return static_cast<int>(colindex);
}

void CplexSolver::AddRow(const row& ThisRow){
   if (lp == NULL) {
      throw std::runtime_error("Cannot add a row before reading a CPLEX problem");
   }
   if (ThisRow.Coef.size() != ThisRow.Index.size()) {
      throw std::runtime_error("Cannot add CPLEX row: coefficient/index size mismatch");
   }
   CPXNNZ rmatbeg = 0;
   CPXNNZ nzcnt = static_cast<CPXNNZ>(ThisRow.Coef.size());
   if (!ThisRow.IsIndicator) {
      status = CPXXaddrows(env, lp, 0, 1, nzcnt, &ThisRow.rhs, &ThisRow.sense,
                           &rmatbeg, ThisRow.Index.data(), ThisRow.Coef.data(), NULL, NULL);
      if (status) {
         std::ostringstream oss;
         oss << "Failed to add CPLEX row. CPLEX error code " << status;
         throw std::runtime_error(oss.str());
      }
   } else {
      status = CPXXaddindconstr(env, lp, ThisRow.IndicatorVariable, ThisRow.complemented,
                                nzcnt, ThisRow.rhs, ThisRow.sense,
                                ThisRow.Index.data(), ThisRow.Coef.data(), NULL);
      if (status) {
         std::ostringstream oss;
         oss << "Failed to add CPLEX indicator row. CPLEX error code " << status;
         throw std::runtime_error(oss.str());
      }
   }
}

void CplexSolver::AddAllNewRows(){
   for (const row& ThisRow : LazyRows) {
      AddRow(ThisRow);
   }
   LazyRows.clear();
}

void CplexSolver::ReadProblem(char* name){
   if (lp != NULL) {
      status = CPXXfreeprob(env, &lp);
      if (status) {
         fprintf(stderr, "CPXXfreeprob failed before reading a new problem, error code %d.\n", status);
         exit(EXIT_FAILURE);
      }
   }
   lp = CPXXcreateprob (env, &status, name);

   /* A returned pointer of NULL may mean that not enough memory
      was available or there was some other problem.  In the case of 
      failure, an error message will have been written to the error 
      channel from inside CPLEX.  In this example, the setting of
      the parameter CPXPARAM_ScreenOutput causes the error message to
      appear on stdout.  Note that most CPLEX routines return
      an error code to indicate the reason for failure.   */

   if ( lp == NULL ) {
      fprintf (stderr, "Failed to create LP.\n");
      exit (EXIT_FAILURE);
   }

   /* Now read the file, and copy the data into the created lp */

   status = CPXXreadcopyprob (env, lp, name, NULL);
   if ( status ) {
      fprintf (stderr, "Failed to read and copy the problem data.\n");
      exit (EXIT_FAILURE);
   } 
}

void CplexSolver::ExtractDR(DR_Holder & DR){
   cur_numcols = CPXXgetnumcols (env, lp);

   /* Allocate space for solution */

   free_and_null ((char **) &x);
   x = (double*) malloc (cur_numcols*sizeof(*x));

   if ( x == NULL ) {
      fprintf (stderr, "No memory for solution values.\n");
      exit (EXIT_FAILURE);
   }

   status = CPXXgetx (env, lp, x, 0, cur_numcols-1);
   if ( status ) {
      fprintf (stderr, "Failed to obtain solution.\n");
      exit (EXIT_FAILURE);
   }


   char          **cur_colname = NULL;
   char          *cur_colnamestore = NULL;
   CPXSIZE       cur_colnamespace;
   CPXSIZE       surplus;


   status = CPXXgetcolname (env, lp, NULL, NULL, 0, &surplus, 0,
                           cur_numcols-1);
   if (( status != CPXERR_NEGATIVE_SURPLUS ) &&
       ( status != 0 )                         )  {
      fprintf (stderr, 
               "Could not determine amount of space for column names.\n");
      exit(0);
   }
   cur_colnamespace = - surplus;
   if ( cur_colnamespace > 0 ) {
      cur_colname      = (char**) malloc (sizeof(*cur_colname)*cur_numcols);
      cur_colnamestore = (char*) malloc (sizeof(*cur_colnamestore)*cur_colnamespace);
      if ( cur_colname      == NULL ||
           cur_colnamestore == NULL   ) {
         fprintf (stderr, "Failed to get memory for column names.\n");
         status = -1;
         exit(0);
      }
      status = CPXXgetcolname (env, lp, cur_colname, cur_colnamestore, 
                              cur_colnamespace, &surplus, 0, cur_numcols-1);
      if ( status ) {
         fprintf (stderr, "CPXXgetcolname failed.\n");
         exit(0);      }
   }
      char          *basismsg;

   for (j = 0; j < cur_numcols; j++) {
      if ( cur_colnamespace > 0 && cur_colname[j][0]=='y' &&(cur_colname[j][1]=='T' || cur_colname[j][1]=='B') ) {
         if(0){printf ("%-16s:  ", cur_colname[j]);
         printf ("%17.10g", x[j]);
         printf ("\n");}
         if(cur_colname[j][2]=='_'){
            DR.set((std::string)cur_colname[j],x[j]);
         }
         else DR.setConst((std::string)cur_colname[j],x[j]);
      }
      else if(cur_colnamespace > 0 && cur_colname[j][0]=='a' && cur_colname[j][1]=='i' && std::fabs(x[j] - 1.0) <= DR_Holder::BinaryTolerance ) DR.SetFirstStageBar((std::string)cur_colname[j]);
     
   }
   DR.SetOptimalValue(objval);
}

bool CplexSolver::ExtractScenario(DR_Holder & DR){
   cur_numcols = CPXXgetnumcols (env, lp);

   /* Allocate space for solution */

   free_and_null ((char **) &x);
   x = (double*) malloc (cur_numcols*sizeof(*x));

   if ( x == NULL ) {
      fprintf (stderr, "No memory for solution values.\n");
      exit (EXIT_FAILURE);
   }

   status = CPXXgetx (env, lp, x, 0, cur_numcols-1);
   if ( status ) {
      fprintf (stderr, "Failed to obtain solution.\n");
      return false;
   }


   char          **cur_colname = NULL;
   char          *cur_colnamestore = NULL;
   CPXSIZE       cur_colnamespace;
   CPXSIZE       surplus;


   status = CPXXgetcolname (env, lp, NULL, NULL, 0, &surplus, 0,
                           cur_numcols-1);
   if (( status != CPXERR_NEGATIVE_SURPLUS ) &&
       ( status != 0 )                         )  {
      fprintf (stderr, 
               "Could not determine amount of space for column names.\n");
      exit(0);
   }
   cur_colnamespace = - surplus;
   if ( cur_colnamespace > 0 ) {
      cur_colname      = (char**) malloc (sizeof(*cur_colname)*cur_numcols);
      cur_colnamestore = (char*) malloc (sizeof(*cur_colnamestore)*cur_colnamespace);
      if ( cur_colname      == NULL ||
           cur_colnamestore == NULL   ) {
         fprintf (stderr, "Failed to get memory for column names.\n");
         status = -1;
         exit(0);
      }
      status = CPXXgetcolname (env, lp, cur_colname, cur_colnamestore, 
                              cur_colnamespace, &surplus, 0, cur_numcols-1);
      if ( status ) {
         fprintf (stderr, "CPXXgetcolname failed.\n");
         exit(0);      }
   }
      char          *basismsg;

   DR.PrepareForScenarioCollection();
   for (j = 0; j < cur_numcols; j++) {
      if ( cur_colnamespace > 0 && cur_colname[j][0]=='z' &&cur_colname[j][1]=='_' ) {
         if(0){printf ("%-16s:  ", cur_colname[j]);
         printf ("%17.10g", x[j]);
         printf ("\n");}
         DR.CollectScenario((std::string)cur_colname[j],x[j]);
      }     
   }
   return DR.BuildAndAddScenarioString();
}


void CplexSolver::PrintSolution(){
   cur_numcols = CPXXgetnumcols (env, lp);

   /* Allocate space for solution */

   free_and_null ((char **) &x);
   x = (double*) malloc (cur_numcols*sizeof(*x));

   if ( x == NULL ) {
      fprintf (stderr, "No memory for solution values.\n");
      exit (EXIT_FAILURE);
   }

   status = CPXXgetx (env, lp, x, 0, cur_numcols-1);
   if ( status ) {
      fprintf (stderr, "Failed to obtain solution.\n");
      return;
   }

   /* Write out the solution */
   CPXDIM index_p;

   status = CPXXgetcolindex(env, lp, "f_0", &index_p);
   printf("Status is %d, Num is %d\n",status, index_p);



   char          **cur_colname = NULL;
   char          *cur_colnamestore = NULL;
   CPXSIZE       cur_colnamespace;
   CPXSIZE       surplus;


status = CPXXgetcolname (env, lp, NULL, NULL, 0, &surplus, 0,
                           cur_numcols-1);

   if (( status != CPXERR_NEGATIVE_SURPLUS ) &&
       ( status != 0 )                         )  {
      fprintf (stderr, 
               "Could not determine amount of space for column names.\n");
      exit(0);
   }

   cur_colnamespace = - surplus;
   if ( cur_colnamespace > 0 ) {
      cur_colname      = (char**) malloc (sizeof(*cur_colname)*cur_numcols);
      cur_colnamestore = (char*) malloc (sizeof(*cur_colnamestore)*cur_colnamespace);
      if ( cur_colname      == NULL ||
           cur_colnamestore == NULL   ) {
         fprintf (stderr, "Failed to get memory for column names.\n");
         status = -1;
         exit(0);
      }
      status = CPXXgetcolname (env, lp, cur_colname, cur_colnamestore, 
                              cur_colnamespace, &surplus, 0, cur_numcols-1);
      if ( status ) {
         fprintf (stderr, "CPXXgetcolname failed.\n");
         exit(0);      }
   }
      char          *basismsg;

   if(0)for (j = 0; j < cur_numcols; j++) {
      if ( cur_colnamespace > 0 ) {
         printf ("%-16s:  ", cur_colname[j]);
      }
      else {
         printf ("Fake%-6.6d      :  ", j);;
      }
      printf ("%17.10g", x[j]);
      printf ("\n");
   }
}
double CplexSolver::Solve(bool PrintOutput){
   /* Optimize the problem and obtain solution. */
   lastSolutionStatus = 0;
   lastObjectiveAvailable = false;

   int status = 0;

   if(PrintOutput) CPXXsetintparam (env, CPXPARAM_ScreenOutput, CPX_ON);
   else CPXXsetintparam (env, CPXPARAM_ScreenOutput, CPX_OFF);

   status = CPXXmipopt (env, lp);

   if ( status ) {
      std::ostringstream oss;
      oss << "Failed to optimize MIP. CPLEX error code " << status;
      throw std::runtime_error(oss.str());
   }

   lastSolutionStatus = CPXXgetstat(env, lp);

   solstat = CPXXgetstat (env, lp);
   std::cerr << "CPLEX solution status " << solstat << ": " << statusToString(solstat) << std::endl;

   double objValue = std::numeric_limits<double>::quiet_NaN();
   status = CPXXgetobjval(env, lp, &objValue);

   if (status) {
       if (IsInfeasible() || IsTimeLimit()) {
           if (PrintOutput) {
               std::cerr
                   << "No objective value available for CPLEX status "
                   << lastSolutionStatus
                   << " (CPLEX error code " << status << ").\n";
           }
           return std::numeric_limits<double>::quiet_NaN();
       }

       std::ostringstream oss;
       oss << "Failed to obtain objective value. Solver status "
           << lastSolutionStatus
           << ", CPLEX error code " << status;
       throw std::runtime_error(oss.str());
   }

   lastObjectiveAvailable = true;
   return objValue;
} 

void CplexSolver::Clear(){
     /* Free up the solution */

   free_and_null ((char **) &x);

   /* Free up the problem as allocated by CPXXcreateprob, if necessary */

   if ( lp != NULL ) {
      status = CPXXfreeprob (env, &lp);
      if ( status ) {
         fprintf (stderr, "CPXXfreeprob failed, error code %d.\n", status);
      }
   }

   /* Free up the CPLEX environment, if necessary */

   if ( env != NULL ) {
      status = CPXXcloseCPLEX (&env);

      /* Note that CPXXcloseCPLEX produces no output,
         so the only way to see the cause of the error is to use
         CPXXgeterrorstring.  For other CPLEX routines, the errors will
         be seen if the CPXPARAM_ScreenOutput indicator is set to CPX_ON. */

      if ( status ) {
      char  errmsg[CPXMESSAGEBUFSIZE];
         fprintf (stderr, "Could not close CPLEX environment.\n");
         CPXXgeterrorstring (env, status, errmsg);
         fprintf (stderr, "%s", errmsg);
      }
   }
   std::cerr <<"Done with clear" <<std::endl;
     
}
