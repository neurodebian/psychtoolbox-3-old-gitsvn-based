
/*
  PsychToolbox2/Source/Common/PsychOctaveScriptingGlue.c		
  
  AUTHORS:
  Allen.Ingling@nyu.edu		awi 
  
  PLATFORMS: All -- Glue layer for GNU/Octave runtime environment.
  
  HISTORY:
  05/07/06 	mk	Derived from Allen's PsychScriptingGlue
  
  DESCRIPTION:
  
	ScriptingGlue defines abstracted functions to pass values 
	between the calling GNU/Octave environment and the PsychToolbox. 
  
  NOTES:
  
  About default arguments:  In previous versions of the Psychtoolbox any matrix of size m*n=0 
  stood for the the "default" matrix.  When passed as an argument, it indicated that the
  default value for that argument should be used.  This is useful when "omitting" intervening
  arguments. 
  
  Because each SCREEN subfunction interpreted arguments independently the ambiguities which 
  are discussed below did not have to be addressed but in the subfunctions which they arrose. 
  (which might be none).  The introduction of abstracted functions in ScriptingGlue mandates 
  a uniform policy for resloving ambiguities.   
  
  Sometimes we want to pass an argument of size 0x0 and mean argument of size 0x0, not the 
  default matrix.  So ScriptingGlue functions which retrieve input arguments can not safetly 
  interpret an empty matrix as the default matrix.
  
  The problem is not as bad as it seems, because we can pass an empty 
  numerical matrix, "[]" when a string argument is expected, or pass an empty string "''" when 
  a numerical argument is expected.  Only in the case when an argument may be either a string or a number,
  and 0 size arguments of both types are meaningful do we have a problem.  The case does not seem likely ever
  to arise. 
  
  For users, having two default arguments, '' and [],  and having to decide which to use depending on the 
  type of argument accepted, complicates the use of default arguments unpleasantly.  Furthermore, empty strings
  are meaninful as strings, but empty numerical matrices are rarely meaninful as matrices. (why is that?)
  Therefore, the best policy for ScriptingGlue functions would be: ScriptingGlue  functions which 
  retrieve string arguments will only interpret [] as the default matrix and will interpret '' as
  the empty string.  ScriptingGlue functions which retrieve numerical arguments will accept either
  [] or '' to be the empty string.  
  
  So [] when passed for a number is always interpreted as the default matrix,  
  [] is the only value which stands for default when passed for a string,  Therefore, we can 
  reduce this further and accept only [] to stand for default, simplifing the users's decision of when to
  use '' and when to use [], by ALWAYS using [].  
  
  So in conclusion:
   -[] and only [] means the default matrix. 
   -If you want a user to pass [] to mean a 0x0 matrix, too bad, you can't do that.  
   All ScriptingGlue functions will report that the argument was not present if the user
   passes [].    
  
       
  
  TO DO: 
    
    - baseFunctionInvoked and PsychSubfunctionEnabled are redundent, keep only baseFunctionInvoked
  	
        
    Less Important:
    
        -Expand for use with N dimensional arrays.  
  	The final required argument to these functions should be the number of dimensions and 
  	thereafter optional function arguments can give the size of each dimension.
  	
  	

*/

// I dont know why, byt it is absolutely critical that octave/oct.h is included
// before *any* other header file, esp. Psych.h, otherwise the C++ compiler f%%2!s up
// completely!
#include <octave/oct.h>

#include "Psych.h"

#ifdef PTBOCTAVE

#if defined(__cplusplus)
//extern "C" {
#endif


// Special hacks to allow Psychtoolbox to build for GNU-OCTAVE:
#if PSYCH_LANGUAGE == PSYCH_OCTAVE

#include <string.h>
#include <setjmp.h>

//#include <octave/ov-scalar.h>
//#include <octave/parse.h>
//#include <octave/Array.h>
//#include <octave/mx-base.h>
//#include <octave/ov-base.h>
////#include <octave/ov-typeinfo.h>
////#include <octave/ov-int8.h>

// This jump-buffer stores CPU- and stackstate at the position
// where our octFunction() dispatcher actually starts executing
// the functions or subfunctions code. That is the point where
// a PsychErrorExit() or PsychErrorExitMsg() will return control...
jmp_buf jmpbuffer;

// Error exit handler: Replacement for Matlabs MEX-handler:
// Prints the error-string with Octaves printing facilities,
// sets Octave error state and longjmp's to the cleanup routine
// at the end of our octFunction dispatcher...
void mexErrMsgTxt(const char* s) {
  if (s && strlen(s)>0) {
    error("%s: %s", mexFunctionName,s);
  }
  else error("");

  // Use the jump-buffer to unwind the stack...
  longjmp(jmpbuffer, 1);
}

void mexPrintf(const char* fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  octave_vformat(octave_diary, fmt, args);
  octave_vformat(octave_stdout, fmt, args);
  va_end(args);
}

void* mxMalloc(int size)
{
  return(malloc(size));
}

void* mxCalloc(int size, int numelements)
{
  return(calloc(size, numelements));
}

void mxFree(void* p)
{
  free(p);
}

// Our implementation: Only accepts 2D matrices. Aborts on anything else. Always creates
// double matrices of real type.
mxArray* mxCreateNumericArray(int numDims, int dimArray[], int arraytype, int realorcomplex)
{
  mxArray* retval;
  int rows, cols, layers;
  if (numDims>2) PsychErrorExitMsg(PsychError_unimplemented, "FATAL Error: mxCreateNumericArray: > 3D Matrices are not supported yet on GNU/Octave build!");
  rows = dimArray[0];
  cols = (numDims>1) ? dimArray[1] : 1;
  layers = (numDims>2) ? dimArray[2] : 1;

  // Allocate our mxArray-Struct:
  retval = (mxArray*) malloc(sizeof(mxArray));
  if (retval==NULL) PsychErrorExitMsg(PsychError_outofMemory, "");

  if (arraytype==mxUINT8_CLASS) {
    PsychErrorExitMsg(PsychError_unimplemented, "FATAL Error: mxCreateNumericArray: UINT8 matrices not yet implemented on Octave.");
  }
  else if (arraytype==mxDOUBLE_CLASS && rows*cols*layers > 1) {
    printf("NEW MATRIX: %i,%i,%i\n", rows, cols, layers); fflush(NULL);
    // Create empty double-matrix of type mxREAL...
    Matrix m(rows, cols);
    // Retrieve a pointer to internal representation. As m is new
    // this won't trigger a deep-copy.
    retval->d = (void*) m.data();
    printf("M-DATA %p\n", retval->d); fflush(NULL);
    // Build a new oct_value object from Matrix m: This is a
    // shallow-copy.
    octave_value* ovp = new octave_value();
    *ovp = m;
    retval->o = (void*) ovp;
    // At this point we can safely destroy Matrix m, as the new
    // octave_object holds a reference to its representation.
  }
  else if (arraytype==mxDOUBLE_CLASS && rows*cols*layers == 1) {
    printf("NEW SCALAR:\n"); fflush(NULL);
    // This is a scalar value:
    retval->o = (void*) new octave_value(0.0);
    double* dp = (double*) PsychMallocTemp(sizeof(double));
    retval->d = (void*) dp;
  }
  else if (arraytype==mxLOGICAL_CLASS) {
    printf("NEW BOOLMATRIX: %i, %i\n", rows, cols, layers); fflush(NULL);
    // Create empty double-matrix of type mxREAL...
    boolMatrix m(rows, cols);
    // Retrieve a pointer to internal representation. As m is new
    // this won't trigger a deep-copy.
    retval->d = (void*) m.data();
    // Build a new oct_value object from Matrix m: This is a
    // shallow-copy.
    octave_value* ovp = new octave_value();
    *ovp = m;
    retval->o = (void*) ovp;
    // At this point we can safely destroy Matrix m, as the new
    // octave_object holds a reference to its representation.
  }
  else {
    PsychErrorExitMsg(PsychError_internal, "FATAL Error: mxCreateNumericArray: Unknown matrix type requested!");
  }

  return(retval);
}

mxArray* mxCreateDoubleMatrix(int rows, int cols, int realorcomplex)
{
  int dims[2];
  dims[0]=rows;
  dims[1]=cols;
  return(mxCreateNumericArray(2, dims, mxDOUBLE_CLASS, mxREAL));  
}

mxArray* mxCreateLogicalMatrix(int rows, int cols)
{
  int dims[2];
  dims[0]=rows;
  dims[1]=cols;
  return(mxCreateNumericArray(2, dims, mxLOGICAL_CLASS, mxREAL));
}

mxArray* mxCreateString(const char* instring)
{
  mxArray* retval;

  // Allocate our mxArray-Struct:
  retval = (mxArray*) malloc(sizeof(mxArray));
  if (retval==NULL) PsychErrorExitMsg(PsychError_outofMemory, "");
  
  retval->d = NULL;
  retval->o = (void*) new octave_value(instring);
  return(retval);
}

void* mxGetData(const mxArray* arrayPtr)
{
  return(arrayPtr->d);
}

double* mxGetPr(const mxArray* arrayPtr)
{
  return((double*) mxGetData(arrayPtr));
}

Boolean* mxGetLogicals(const mxArray* arrayPtr)
{
  return((Boolean*) mxGetData(arrayPtr));
}
#define GETOCTPTR(x) ((octave_value*) (x)->o)
int mxGetM(const mxArray* arrayPtr)
{
  return(GETOCTPTR(arrayPtr)->rows());
}

int mxGetN(const mxArray* arrayPtr)
{
  return(GETOCTPTR(arrayPtr)->columns());
}

int mxGetNumberOfDimensions(const mxArray* arrayPtr)
{
  return(GETOCTPTR(arrayPtr)->ndims());
}

int* mxGetDimensions(const mxArray* arrayPtr)
{
  dim_vector vdim = GETOCTPTR(arrayPtr)->dims();
  int* dims = (int*) PsychMallocTemp(3*sizeof(int));
  dims[0] = vdim(0);
  dims[1] = vdim(1);
  if (GETOCTPTR(arrayPtr)->ndims()>2) dims[2] = vdim(2); else dims[2]=1;
  return(dims);
}

int mxGetString(const mxArray* arrayPtr, char* outstring, int outstringsize)
{
  if (!mxIsChar(arrayPtr)) PsychErrorExitMsg(PsychError_internal, "FATAL Error: Tried to convert a non-string into a string!");
  return(((snprintf(outstring, outstringsize, "%s", GETOCTPTR(arrayPtr)->string_value().c_str()))>0) ? 0 : 1);
}

//MK: FIXME TODO!
mxArray* mxCreateStructArray(int numDims, int* ArrayDims, int numFields, const char** fieldNames)
{
  if (numDims>2) PsychErrorExitMsg(PsychError_unimplemented, "FATAL Error: mxCreateStructArray: 3D Arrays are not supported yet on GNU/Octave build!");
  //  return(mxCreateStructMatrix(ArrayDims[0], (numDims>1) ? ArrayDims[1] : 1, numFields, fieldNames));
  return(NULL);
}

// MK: FIXME TODO!
mxArray* mxCreateCellArray(int numDims, int* ArrayDims)
{
  //  char dummyname[4] = "CAF";
  //char* dummynameptr = &dummyname;
  //char** dummyptr = &dummynameptr;
  if (numDims>2) PsychErrorExitMsg(PsychError_unimplemented, "FATAL Error: mxCreateCellArray: 3D Arrays are not supported yet on GNU/Octave build!");
  //  return(mxCreateStructMatrix(ArrayDims[0], (numDims>1) ? ArrayDims[1] : 1, 1, dummyptr));
  return(NULL);
}

void mxSetCell(PsychGenericScriptType *cellVector, int index, mxArray* mxFieldValue)
{
  return;

  if (!mxIsStruct((mxArray*) cellVector)) {
    PsychErrorExitMsg(PsychError_internal, "FATAL Error: mxSetCell: Tried to manipulate something other than a cell-vector!");
  }

  if (mxGetM((mxArray*) cellVector) * mxGetN((mxArray*) cellVector) <= index) {
    PsychErrorExitMsg(PsychError_internal, "FATAL Error: mxSetCell: Tried to manipulate invalid index in a cell-vector!");
  }

  mxSetField((mxArray*) cellVector, index, "CAF", mxFieldValue);
}

void mxSetLogical(mxArray* dummy)
{
  // FIXME: Todo.
  return;
}

int mxIsLogical(const mxArray* a)
{
  return(GETOCTPTR(a)->is_bool_type());
}

int mxIsCell(const mxArray* a)
{
  return(GETOCTPTR(a)->is_cell());
}

int mxIsNumeric(const mxArray* a)
{
  return(GETOCTPTR(a)->is_numeric_type());
}

int mxIsChar(const mxArray* a)
{
  return(GETOCTPTR(a)->is_char_matrix() || GETOCTPTR(a)->is_string());
}

int mxIsDouble(const mxArray* a)
{
  return(GETOCTPTR(a)->is_real_type());
}

int mxIsUint8(const mxArray* a)
{
  // printf("BYTESIZE %i NUMEL %i\n", GETOCTPTR(a)->byte_size(), GETOCTPTR(a)->numel());

  return((mxIsNumeric(a) && (GETOCTPTR(a)->byte_size() / GETOCTPTR(a)->numel() == 1)) ? TRUE : FALSE);
}

int mxIsUint16(const mxArray* a)
{
  return((mxIsNumeric(a) && (GETOCTPTR(a)->byte_size() / GETOCTPTR(a)->numel() == 2)) ? TRUE : FALSE);
  //  int t=GETOCTPTR(a)->type_id();
  //return(t==octave_uint16_matrix::static_type_id());
}

int mxIsUint32(const mxArray* a)
{
  return((mxIsNumeric(a) && (GETOCTPTR(a)->byte_size() / GETOCTPTR(a)->numel() == 4)) ? TRUE : FALSE);

  //  int t=GETOCTPTR(a)->type_id();
  // return(t==octave_uint32_matrix::static_type_id());
}

int mxIsInt8(const mxArray* a)
{
  return(mxIsUint8(a));
  //  int t=GETOCTPTR(a)->type_id();
  //return(t==octave_int8_matrix::static_type_id());
}

int mxIsInt16(const mxArray* a)
{
  return(mxIsUint16(a));
  //  int t=GETOCTPTR(a)->type_id();
  //return(t==octave_int16_matrix::static_type_id());
}

int mxIsInt32(const mxArray* a)
{
  return(mxIsUint32(a));
  //int t=GETOCTPTR(a)->type_id();
  //return(t==octave_int32_matrix::static_type_id());
}

#endif

////Static functions local to ScriptingGlue.c.  
#if PSYCH_LANGUAGE == PSYCH_MATLAB
void InitializeSynopsis(char *synopsis[],int maxStrings);
#endif 



// _____________________________________________________________________________________
// for Matlab
#if PSYCH_LANGUAGE == PSYCH_MATLAB | PSYCH_LANGUAGE == PSYCH_OCTAVE

#define MAX_SYNOPSIS 100
#define MAX_CMD_NAME_LENGTH 100

//Static variables local to ScriptingGlue.c.  The convention is to append a abbreviation in all
//caps of the C file name to the variable name.   
int nlhsGLUE;  // Number of requested return arguments.
int nrhsGLUE;  // Number of provided call arguments.

#if PSYCH_LANGUAGE == PSYCH_MATLAB
static mxArray **plhsGLUE;       // A pointer to the plhs array passed to the MexFunction entry point  
static CONSTmxArray **prhsGLUE; // A pointer to the prhs array passed to the MexFunction entry point
#endif

#if PSYCH_LANGUAGE == PSYCH_OCTAVE
#define MAX_OUTPUT_ARGS 100
#define MAX_INPUT_ARGS 100
static mxArray* plhsGLUE[MAX_OUTPUT_ARGS]; // An array of pointers to the octave return arguments.
static mxArray* prhsGLUE[MAX_INPUT_ARGS];  // An array of pointers to the octave call arguments.
extern const char *mexFunctionName; // This gets initialized by Octave wrapper to contain our function name.
#endif

static Boolean nameFirstGLUE;
//static PsychFunctionPtr psychExitFunctionGLUE=NULL; 
static Boolean subfunctionsEnabledGLUE=FALSE;
static Boolean baseFunctionInvoked=FALSE;
static void PsychExitGlue(void);

//local function declarations
static Boolean PsychIsEmptyMat(CONSTmxArray *mat);
static Boolean PsychIsDefaultMat(CONSTmxArray *mat);
static int mxGetP(const mxArray *array_ptr);
static int mxGetNOnly(const mxArray *arrayPtr);
static mxArray *mxCreateDoubleMatrix3D(int m, int n, int p);

//declarations for functions exported from code module
#if PSYCH_LANGUAGE == PSYCH_MATLAB
EXP void mexFunction(int nlhs, mxArray *plhs[], int nrhs, CONSTmxArray *prhs[]);
#endif



/*

	Main entry point for Matlab and Octave. Serves as a dispatch and handles
	first time initialization.
	
	EXP is a macro defined within Psychtoolbox source to be nothing
	except on win where it is the declaration which tells the linker to 
	make the function visible from outside the DLL. 
        
        The subfunction dispatcher can operate in either of two modes depending
        on whether the module has registed subfunctions, or only a single "base" 
        function.  
        
        subfunction mode:  
        The examines the  first and second 
        arguments for a string naming a module subfunction.  If it finds in either of those
        two arguments a string naming a module subfunctoin, then it looks up the approproate 
        function pointer and invokes that function.  Before invoking the function the dispatcher
        removes the function name argument form the list of argumnets which was passed to the 
        module.  
                
        base mode:  The dispatcher always invokes the same one subfunction and without
        alterinng the list of arguments.
        
        Modules should now register in subfunction mode to support the build-in 'version' command.
        
*/
#if PSYCH_LANGUAGE == PSYCH_MATLAB
EXP void mexFunction(int nlhs, mxArray *plhs[], int nrhs, CONSTmxArray *prhs[])
#endif
#if PSYCH_LANGUAGE == PSYCH_OCTAVE
EXP octave_value_list octFunction(const octave_value_list& prhs, const int nlhs)
#endif
{
	static Boolean firstTime = TRUE;
	Boolean errorcondition = FALSE;
	Boolean isArgThere[2], isArgEmptyMat[2], isArgText[2], isArgFunction[2];
	PsychFunctionPtr fArg[2], baseFunction;
	char argString[2][MAX_CMD_NAME_LENGTH];
	int i; 
	mxArray* tmparg = NULL; // mxArray is mxArray under MATLAB but #defined to octave_value on OCTAVE build.
	#if PSYCH_LANGUAGE == PSYCH_OCTAVE
	  // plhs is our octave_value_list of return values:
	  octave_value tmpval;      // Temporary, needed in parser below...
	  octave_value_list plhs;   // Our list of left-hand-side return values...
	  int nrhs = prhs.length();
	#endif
 
	// Initialization
	if (firstTime) {
		
		//call the Psychtoolbox init function, which inits the Psychtoolbox and calls the project init. 
		PsychInit();
		
		//register the exit function, which calls PsychProjectExit() to clean up for the project then
		//calls whatever to clean up for all of Psych.h layer.
		#if PSYCH_LANGUAGE == PSYCH_MATLAB
		mexAtExit(&PsychExitGlue);
                #endif

		#if PSYCH_LANGUAGE == PSYCH_OCTAVE
		warning("FIXME: No mexAtExit() handler for GNU/Octave port!!");
                #endif

		firstTime = FALSE;
	}
	
	#if PSYCH_LANGUAGE == PSYCH_OCTAVE
	  // Save CPU-state and stack at this position in 'jmpbuffer'. If any further code
	  // calls an error-exit function like PsychErrorExit() or PsychErrorExitMsg() then
	  // the corresponding longjmp() call in our mexErrMsgTxt() implementation (see top of file)
	  // will unwind the stack and restore stack-state and CPU state to the saved values in
	  // jmpbuffer --> We will end up at this setjmp() call again, with a cleaned up stack and
	  // CPU state, but setjmp will return a non-zero error code, signaling the abnormal abortion.
	  if (setjmp(jmpbuffer)!=0) {
	    // PsychErrorExit() or friends called! The CPU and stack are restored to a sane state.
	    // Call our cleanup-routine to release memory that is PsychMallocTemp()'ed and to other
	    // error-handling...
	    errorcondition = TRUE;
	    goto octFunctionCleanup;
	  }
	#endif

	// Store away call arguments for use by language-neutral accessor functions in ScriptingGlue.c
	#if PSYCH_LANGUAGE == PSYCH_MATLAB
	nlhsGLUE = nlhs;
	nrhsGLUE = nrhs;
	plhsGLUE = plhs;
	prhsGLUE = prhs;
	#endif

	#if PSYCH_LANGUAGE == PSYCH_OCTAVE

	// Setup our prhsGLUE array of call argument pointers:
	// We make copies of prhs to simplify the rest of PsychScriptingGlue. This copy is not
	// as expensive as it might look, because Octave objects are all implemented via
	// "Copy-on-write" --> Only a pointer is copied as long as we don't modify the data.
	// MK: TODO FIXME -- Should we keep an extra array octave_value dummy[MAX_INPUT_ARGS];
	// around, assign to that dummy[i]=prhs(i); and set ptrs to it prhsGLUE[i]=&dummy[i];
	// This would require more memory, but prevent possible heap-fragmentation due to
	// lots of new()/delete() calls on each invocation of the OCT-Function --> possible
	// slow-down over time, could be confused with memory leaks???
	for(int i=0; i<nrhs && i<MAX_INPUT_ARGS; i++) {
	  // Create and assign our mxArray-Struct:
	  prhsGLUE[i] = (mxArray*) malloc(sizeof(mxArray));
	  if (prhsGLUE[i]==NULL) PsychErrorExitMsg(PsychError_outofMemory, "");


	  // Extract data-pointer to each prhs(i) octave_value:
	  if (prhs(i).is_string() || prhs(i).is_char_matrix()) {
	    printf("INPUT %i: STRING\n", i); fflush(NULL);
	    // A string object:
	    // Strings do not have a need for a data-ptr. Just copy the octave_value object...
	    prhsGLUE[i]->d = NULL;
	    prhsGLUE[i]->o = (void*) new octave_value(prhs(i));  // Refcont now >= 2
	    // Done.
	  } 
	  else if (prhs(i).is_real_type() && !prhs(i).is_scalar_type()) {
	    // A double input matrix:               // Refcount now >=1
	    printf("INPUT %i: MATRIX\n", i); fflush(NULL);

	    // Create intermediate representation m: This is a shallow-copy...
	    const Matrix m(prhs(i).matrix_value()); // Refcount now >=2

	    // Get internal dataptr from it:        // This triggers a deep-copy :(
	    prhsGLUE[i]->d = (void*) m.data();      // Refcount now == 1
	    
	    // Create a shallow backup copy of corresponding octave_value...
	    octave_value* ovptr = new octave_value();
	    *ovptr = m;
	    prhsGLUE[i]->o = (void*) ovptr;  // Refcont now == 2
	    
	    // As soon as m gets destructed by leaving this if-branch,
	    // the refcount will drop to == 1...

	    // Done.
	  } else if (prhs(i).is_real_type() && prhs(i).is_scalar_type()) {
	    // A double scalar:
	    printf("INPUT %i: SCALAR\n", i); fflush(NULL);
	    prhsGLUE[i]->o = (void*) new octave_value(prhs(i));
	    double* m = (double*) PsychMallocTemp(sizeof(double));
	    *m = prhs(i).double_value();
	    prhsGLUE[i]->d = (void*) m;
	  }

	}

	// NULL-out our pointer array of return value pointers plhsGLUE:
	memset(&plhsGLUE[0], 0, sizeof(plhsGLUE));

	nrhsGLUE = nrhs;
	nlhsGLUE = nlhs;
	#endif

        baseFunctionInvoked=FALSE;

	//if no subfunctions have been registered by the project then just invoke the project base function
	//if one of those has been registered.
	if(!PsychAreSubfunctionsEnabled()){
		baseFunction = PsychGetProjectFunction(NULL);
		if(baseFunction != NULL){
                        baseFunctionInvoked=TRUE;
			(*baseFunction)();  //invoke the unnamed function
		}else
			PrintfExit("Project base function invoked but no base function registered");
	}else{ //subfunctions are enabled so pull out the function name string and invoke it.
		//assess the nature of first and second arguments for finding the name of the sub function.  
		for(i=0;i<2;i++)
		{
			isArgThere[i] = nrhs>i;
			#if PSYCH_LANGUAGE == PSYCH_MATLAB
			if (isArgThere[i]) tmparg = prhs[i]; else tmparg = NULL;
			#endif

			#if PSYCH_LANGUAGE == PSYCH_OCTAVE
			if (isArgThere[i]) { tmparg = prhsGLUE[i]; } else { tmparg = NULL; }
			#endif

			isArgEmptyMat[i] = isArgThere[i] ? mxGetM(tmparg)==0 || mxGetN(tmparg)==0 : FALSE;  
			isArgText[i] = isArgThere[i] ? mxIsChar(tmparg) : FALSE;
			if(isArgText[i]){
				mxGetString(tmparg,argString[i],sizeof(argString[i]));
				fArg[i]=PsychGetProjectFunction(argString[i]);
			}
			isArgFunction[i] = isArgText[i] ? fArg[i] != NULL : FALSE;
		}
		 


		//figure out which of the two arguments might be the function name and either invoke it or exit with error
		//if we can't find one.  

		if(!isArgThere[0] && !isArgThere[1]){ //no arguments passed so execute the base function 	
			baseFunction = PsychGetProjectFunction(NULL);
			if(baseFunction != NULL){
                                baseFunctionInvoked=TRUE;
				(*baseFunction)();
			}else
				PrintfExit("Project base function invoked but no base function registered");
		}
		// (!isArgThere[0] && isArgEmptyMat[1]) --disallowed
		// (!isArgThere[0] && isArgText[1])     --disallowed
		// (!isArgThere[0] && !isArgText[1]     --disallowed except in case of !isArgThere[0] caught above. 

		else if(isArgEmptyMat[0] && !isArgThere[1])
			PrintfExit("Invalid command (error state A)");
		else if(isArgEmptyMat[0] && isArgEmptyMat[1])
			PrintfExit("Invalid command (error state B)");
		else if(isArgEmptyMat[0] && isArgText[1]){
			if(isArgFunction[1]){
				nameFirstGLUE = FALSE;
				(*(fArg[1]))();
			}
			else
				PrintfExit("Invalid command (error state C)");
		}
		else if(isArgEmptyMat[0] && !isArgText[1])
			PrintfExit("Invalid command (error state D)");
			
		else if(isArgText[0] && !isArgThere[1]){
			if(isArgFunction[0]){
				nameFirstGLUE = TRUE;
				(*(fArg[0]))();
			}else{ //when we receive a first argument  wich is a string and it is  not recognized as a function name then call the default function 
			/*
                        else
				PrintfExit("Invalid command (error state E)");
                        */
                            baseFunction = PsychGetProjectFunction(NULL);
                            if(baseFunction != NULL){
                                baseFunctionInvoked=TRUE;
				(*baseFunction)();
                            }else
				PrintfExit("Project base function invoked but no base function registered");
                        }
                            
		}
		else if(isArgText[0] && isArgEmptyMat[1]){
			if(isArgFunction[0]){
				nameFirstGLUE = TRUE;
				(*(fArg[0]))();
			}
			else
				PrintfExit("Invalid command (error state F)");
		}
		else if(isArgText[0] && isArgText[1]){
			if(isArgFunction[0] && !isArgFunction[1]){ //the first argument is the function name
				nameFirstGLUE = TRUE;
				(*(fArg[0]))();
			}
			else if(!isArgFunction[0] && isArgFunction[1]){ //the second argument is the function name
				nameFirstGLUE = FALSE;
				(*(fArg[1]))();
			}
			else if(!isArgFunction[0] && !isArgFunction[1]){ //neither argument is a function name
                            //PrintfExit("Invalid command (error state G)");
                            baseFunction = PsychGetProjectFunction(NULL);
                            if(baseFunction != NULL){
                                baseFunctionInvoked=TRUE;
				(*baseFunction)();
                            }else
				PrintfExit("Project base function invoked but no base function registered");
                        }
			else if(isArgFunction[0] && isArgFunction[1]) //both arguments are function names
				PrintfExit("Passed two function names");
		}
		else if(isArgText[0] && !isArgText[1]){
			if(isArgFunction[0]){
				nameFirstGLUE = TRUE;
				(*(fArg[0]))();
			}
			else
				PrintfExit("Invalid command (error state H)");
		}

		else if(!isArgText[0] && !isArgThere[1]){  //this was modified for MODULEVersion with WaitSecs.
                    //PrintfExit("Invalid command (error state H)");
                    baseFunction = PsychGetProjectFunction(NULL);
                    if(baseFunction != NULL){
                        baseFunctionInvoked=TRUE;
                        (*baseFunction)();  //invoke the unnamed function
                    }else
                        PrintfExit("Project base function invoked but no base function registered");
                }
		else if(!isArgText[0] && isArgEmptyMat[1])
			PrintfExit("Invalid command (error state I)");
		else if(!isArgText[0] && isArgText[1])
		{
			if(isArgFunction[1]){
				nameFirstGLUE = FALSE;
				(*(fArg[1]))();
			}
			else
				PrintfExit("Invalid command (error state J)");
		}
		else if(!isArgText[0] && !isArgText[1]){  //this was modified for Priority.
                    //PrintfExit("Invalid command (error state K)");
                    baseFunction = PsychGetProjectFunction(NULL);
                    if(baseFunction != NULL){
                        baseFunctionInvoked=TRUE;
                        (*baseFunction)();  //invoke the unnamed function
                    }else
                        PrintfExit("Project base function invoked but no base function registered");
                }

	} //close else			

	// If we reach this point of execution under Matlab, then we're successfully done.

#if PSYCH_LANGUAGE == PSYCH_OCTAVE
	// If we reach this point of execution under Octave, then we're done, possibly due to
	// error abort. Let's first do the memory management cleanup work necessary on Octave.
	// This is either done due to successfull execution or via jump to octFunctionCleanup:
	// in case of error-abort:
octFunctionCleanup:

	// Release our own prhsGLUE array...
	// FIXME: Release memory for scalar types... -- Malloc cleanup handler should take care of this...
	for(int i=0; i<nrhs && i<MAX_INPUT_ARGS; i++) if(prhsGLUE[i]) {
	  delete(((octave_value*)(prhsGLUE[i]->o)));
	  free(prhsGLUE[i]);
	  prhsGLUE[i]=NULL;	  
	}

	// Return our octave_value_list of returned values in any case and yield control
	// back to Octave:

	// "Copy" our octave-value's into the output array:
	for(i=0; i<nlhs && i<MAX_OUTPUT_ARGS; i++) {
	  if (plhsGLUE[i]) {
	    plhs(i) = *((octave_value*)(plhsGLUE[i]->o));
	    if (plhs(i).is_scalar_type()) {
	      // Special case: Scalar. Need to override with our double-ptrs value:
	      double* svalue = (double*) plhsGLUE[i]->d;
	      plhs(i) = octave_value((double) *svalue);
	    }

	    free(plhsGLUE[i]);
	    plhsGLUE[i]=NULL;
	  }
	}

	// Release all memory allocated via PsychMallocTemp():
	// MK: FIXME TODO...

	// Is this a successfull return?
	if (errorcondition) {
	  // Nope - Error return!
	  std::string cleancommand("Screen('CloseAll');");
	  int parse_status=0;
	  // FIXME! eval_string(cleancommand, TRUE, parse_status);
	}

	// Return it:
	return(plhs);
#endif
}


/*
	Just call the abstracted PsychExit function.  This might seem dumb, but its necessary to 
	isolate the scripting language dependent stuff from the rest of the toolbox.  
	
*/
void PsychExitGlue(void)
{
	PsychErrorExitMsg(PsychExit(),NULL);
}	


/*
	Return the mxArray pointer to the specified position.  Note that we have some special rules for 
	for numbering the positions: 
	
	0 - This is always the command string or NULL if the project does not register a 
	    dispatch function and does accept subcommands.  If the function does accept sub
	    commands, in Matlab those may be passed in either the first or second position, but
	    PsychGetArgPtr() will always return the command as the 0th. 
	    
	1 - This is the first argument among the arguments which are not the subfunction name itself.
	    It can occur in either the first or second position of the argument list, depending on
	    in which of those two positions the function name itself appears.
	    
	2.. These positions are numbered correctly 
	
	TO DO:  
	
	
	2 - this function should be used by the one which gets the function name.    
		

	Arguments are numbered 0..n.  

		-The 0th argument is a pointer to the mxArray holding
		the subfunction name string if we are in subfunction mode.  
	
		-The 0th argument is undefined if not in subfunction mode.  
		
		-The 1st argument is the argument of the 1st and 2nd which is not
		 the subfunction name if in subfunction mode.
		 
		-The 1st argument is the first argument if not in subfunction mode.
		
		-The 2nd-nth arguments are always the 2nd-nth arguments. 		
*/
//we return NULL if a postion without an arg is specified.
const mxArray *PsychGetInArgMxPtr(int position)
{	


	if(PsychAreSubfunctionsEnabled() && !baseFunctionInvoked){ //when in subfunction mode
		if(position < nrhsGLUE){ //an argument was passed in the correct position.
			if(position == 0){ //caller wants the function name argument.
				if(nameFirstGLUE)
					return(prhsGLUE[0]);
				else
					return(prhsGLUE[1]);
			}else if(position == 1){ //they want the "first" argument.    
				if(nameFirstGLUE)
					return(prhsGLUE[1]);
				else
					return(prhsGLUE[0]);
			}else
				return(prhsGLUE[position]);
		}else
			return(NULL); 
	}else{ //when not in subfunction mode and the base function is not invoked.  
		if(position <= nrhsGLUE)
			return(prhsGLUE[position-1]);
		else
			return(NULL);
	}
}

mxArray **PsychGetOutArgMxPtr(int position)
{	

	if(position==1 || (position>0 && position<=nlhsGLUE)){ //an ouput argument was supplied at the specified location
		return(&(plhsGLUE[position-1]));
	}else
		return(NULL);
}

/*
	functions for enabling and testing subfunction mode
*/
void PsychEnableSubfunctions(void)
{
	subfunctionsEnabledGLUE = TRUE;
}


boolean PsychAreSubfunctionsEnabled(void)
{
	return(subfunctionsEnabledGLUE);
}


/*
	Get the third array dimension which we call "P".  mxGetP should act just like mxGetM and mxGetN.
        	
	The abstracted Psychtoolbox API supports matrices with up to 3 dimensions.     
*/
static int mxGetP(const mxArray *arrayPtr)
{
	const int  *dimArray;
	
	if(mxGetNumberOfDimensions(arrayPtr)<3)
		return(1);
	dimArray=mxGetDimensions(arrayPtr);
	return(dimArray[2]);
}


/*
	Get the 2nd array dimension.
        
	The Mex API's mxGetN is sometimes undersirable because it returns the product of all dimensions above 1.  Our mxGetNOnly only returns N, for when you need that.       
	
	The abstracted Psychtoolbox API supports matrices with up to 3 dimensions.     
*/
static int mxGetNOnly(const mxArray *arrayPtr)
{
	const int  *dimArray;
	
	dimArray=mxGetDimensions(arrayPtr);
	return(dimArray[1]);
}


/*
    mxCreateDoubleMatrix3D()
    
    Create a 2D or 3D matrix of doubles. 
	
    Requirements are that m>0, n>0, p>=0.  
*/
mxArray *mxCreateDoubleMatrix3D(int m, int n, int p)
{
	int numDims, dimArray[3];
	
        if(m==0 || n==0 ){
            dimArray[0]=0;dimArray[1]=0;dimArray[2]=0;	//this prevents a 0x1 or 1x0 empty matrix, we want 0x0 for empty matrices. 
        }else{
            dimArray[0]=m;dimArray[1]=n;dimArray[2]=p;
        }
	numDims= (p==0 || p==1) ? 2 : 3;
	return(mxCreateNumericArray(numDims, dimArray, mxDOUBLE_CLASS, mxREAL));		
}

/*
    mxCreateNativeBooleanMatrix3D()
    
    Create a 2D or 3D matrix of native boolean types. 
	
    Requirements are that m>0, n>0, p>=0.  
*/
mxArray *mxCreateNativeBooleanMatrix3D(int m, int n, int p)
{
	int			numDims, dimArray[3];
	mxArray		*newArray;
	
        if(m==0 || n==0 ){
            dimArray[0]=0;dimArray[1]=0;dimArray[2]=0;	//this prevents a 0x1 or 1x0 empty matrix, we want 0x0 for empty matrices. 
        }else{
            dimArray[0]=m;dimArray[1]=n;dimArray[2]=p;
        }
	numDims= (p==0 || p==1) ? 2 : 3;
	newArray=mxCreateNumericArray(numDims, dimArray, mxLOGICAL_CLASS, mxREAL);

	#if mxLOGICAL_CLASS == mxUINT8_CLASS
        #if PSYCH_SYSTEM == PSYCH_LINUX
	// Manually set the flag to logical for Matlab versions < 6.5
	mxSetLogical(newArray);
	#endif
        #endif
        
	return(newArray);		
}


/*
	Create a 2D or 3D matrix of ubytes.  
	
	Requirements are that m>0, n>0, p>=0.  
*/
mxArray *mxCreateByteMatrix3D(int m, int n, int p)
{
	int numDims, dimArray[3];
	
        if(m==0 || n==0 ){
            dimArray[0]=0;dimArray[1]=0;dimArray[2]=0; //this prevents a 0x1 or 1x0 empty matrix, we want 0x0 for empty matrices.
        }else{
            dimArray[0]=m;dimArray[1]=n;dimArray[2]=p;
        }
	numDims= (p==0 || p==1) ? 2 : 3;
	return(mxCreateNumericArray(numDims, dimArray, mxUINT8_CLASS, mxREAL));
		
} 
 


/*
	Print string s and return return control to the calling environment
*/
void PsychErrMsgTxt(char *s)
{
	mexErrMsgTxt(s);
}


/*
	classify the mxArray element format using Pyschtoolbox argument type names
	
*/
static PsychArgFormatType PsychGetTypeFromMxPtr(const mxArray *mxPtr)
{
	PsychArgFormatType format;

	if(PsychIsDefaultMat(mxPtr))
		format = PsychArgType_default;
	else if(mxIsUint8(mxPtr))
		format = PsychArgType_uint8;
	else if(mxIsUint16(mxPtr))
		format = PsychArgType_uint16;
	else if(mxIsUint32(mxPtr))
		format = PsychArgType_uint32;
	else if(mxIsInt8(mxPtr))
		format = PsychArgType_int8;
	else if(mxIsInt16(mxPtr))
		format = PsychArgType_int16;
	else if(mxIsInt32(mxPtr))
		format = PsychArgType_int32;
	else if(mxIsDouble(mxPtr))
		format = PsychArgType_double;
	else if(mxIsChar(mxPtr))
		format = PsychArgType_char;
	else if(mxIsCell(mxPtr))
		format = PsychArgType_cellArray;
	else if(mxIsLogical(mxPtr))
		format = PsychArgType_boolean;  // This is tricky because MATLAB abstracts "logicals" conditionally on platform. Depending on OS, MATLAB implements booleans with either 8-bit or 64-bit values.  
	else 
		format = PsychArgType_unclassified;

	return format;	
			
}


/*
    PsychSetReceivedArgDescriptor()
    
    Accept an argument number and direction value (input or output).  Examine the specified argument and fill in an argument
    descriptor struture.  Ask a retainer function to store the descriptor. 
    
*/
PsychError PsychSetReceivedArgDescriptor(int 			argNum, 
                                                PsychArgDirectionType 	direction)
{
	PsychArgDescriptorType d;
	int numNamedOutputs, numOutputs;
		
	const mxArray *mxPtr;

	d.position = argNum;
	d.direction = direction;	
	if(direction == PsychArgIn){
		mxPtr = PsychGetInArgMxPtr(argNum);
		d.isThere = (mxPtr && !PsychIsDefaultMat(mxPtr)) ? kPsychArgPresent : kPsychArgAbsent; 
		if(d.isThere == kPsychArgPresent){ //the argument is there so fill in the rest of the description
			d.numDims = mxGetNumberOfDimensions(mxPtr);
			d.mDimMin = d.mDimMax = mxGetM(mxPtr);
			d.nDimMin = d.nDimMax = mxGetNOnly(mxPtr);
			d.pDimMin = d.pDimMax = mxGetP(mxPtr);
			d.type = PsychGetTypeFromMxPtr(mxPtr);
		}
	}
	else{ //(direction == PsychArgOut)
		numNamedOutputs = PsychGetNumNamedOutputArgs();
		numOutputs = PsychGetNumOutputArgs();
		if(numNamedOutputs >=argNum)
			d.isThere = kPsychArgPresent;
		else if(numOutputs >=argNum)
			d.isThere = kPsychArgFixed;
		else
			d.isThere = kPsychArgAbsent;
	}
	PsychStoreArgDescriptor(NULL,&d);
	return(PsychError_none);	
							
}

PsychError PsychSetSpecifiedArgDescriptor(	int			position,
                                                        PsychArgDirectionType 	direction,
                                                        PsychArgFormatType 	type,
                                                        PsychArgRequirementType	isRequired,
                                                        int			mDimMin,		// minimum minimum is 1   |   
                                                        int			mDimMax, 		// minimum maximum is 1, maximum maximum is -1 meaning infinity
                                                        int			nDimMin,		// minimum minimum is 1   |   
                                                        int			nDimMax,		// minimum maximum is 1, maximum maximum is -1 meaning infinity
                                                        int 		pDimMin,	    // minimum minimum is 0
                                                        int			pDimMax)		// minimum maximum is 0, maximum maximum is -1 meaning infinity
{

	PsychArgDescriptorType d;

	d.position = position;
	d.direction = direction;
	d.type = type;
	//d.isThere 			//field set only in the received are descriptor, not in the specified argument descriptor
        d.isRequired = isRequired;	//field set only in the specified arg descritor, not in the received argument descriptot.
	d.mDimMin = mDimMin;
	d.mDimMax = mDimMax;
	d.nDimMin = nDimMin;
	d.nDimMax = nDimMax;
	d.pDimMin = pDimMin;
	d.pDimMax = pDimMax;
        //NOTE that we are not setting the d.numDims field because that is inferred from pDimMin and pDimMax and the 3 dim cap.  
	PsychStoreArgDescriptor(&d,NULL);
	return(PsychError_none);
}


/*
PsychError PsychSetSpecifiedArgDescriptor_old(	int			position,
                                                        PsychArgDirectionType 	direction,
                                                        PsychArgFormatType 	type,
                                                        PsychArgPresenceType	isThere,
                                                        int			mDimMin,		// minimum minimum is 1   |   
                                                        int			mDimMax, 		// minimum maximum is 1, maximum maximum is -1 meaning infinity
                                                        int			nDimMin,		// minimum minimum is 1   |   
                                                        int			nDimMax,		// minimum maximum is 1, maximum maximum is -1 meaning infinity
                                                        int 			pDimMin,		// minimum minimum is 0
                                                        int			pDimMax)		// minimum maximum is 0, maximum maximum is -1 meaning infinity
{

	PsychArgDescriptorType d;

	d.position = position;
	d.direction = direction;
	d.type = type;
	d.isThere = isThere;		
	d.mDimMin = mDimMin;
	d.mDimMax = mDimMax;
	d.nDimMin = nDimMin;
	d.nDimMax = nDimMax;
	d.pDimMin = pDimMin;
	d.pDimMax = pDimMax;
        //NOTE that we are not setting the d.numDims field because that is inferred from pDimMin and pDimMax and the 3 dim cap.  
		
	PsychStoreArgDescriptor(&d,NULL);
	return(PsychError_none);
}
*/


/*
	PsychAcceptInputArgumentDecider()
	
	This is a subroutine of Psychtoolbox functions such as PsychCopyInDoubleArg() which read in arguments to Psychtoolbox functino 
	passed from the scripting environment.  
	
	Accept one constant specifying whether an argument is either required, optional, or anything will be allowed and another constant
	specifying how the provided argument agrees with the specified argument.  Based on the relationship between those constants either:
	
		� Return TRUE indicating that the caller should read in the argument and itself return TRUE to indicate that the argument has been read.
		� Return FALSE indicating that the caller should ignore the argument and itself return FALSE to indicate that the argument was not read.
		� Exit to the calling environment with an error to indicate that the provided argument did not match the requested argument and that
		it was required to match.
		
	
	The domain of supplied arguments is: 
	
	matchError:
		PsychError_internal					-Internal Psychtoolbox error
		PsychError_invalidArg_absent		-There was no argument provided
		PsychError_invalidArg_type			-The argument was present but not the specified type
		PsychError_invalidArg_size			-The argument was presnet and the specified type but not the specified size
		PsychError_none						-The argument matched the specified argument
		
	isRequired:
		kPsychArgRequired					- the argument must be present and must match the specified descriptor
		kPsychArgOptional					- the argument must either be absent or must be present and match the specified descriptor
		kPsychArgAnything					- the argument can be absent or anything

*/
Boolean PsychAcceptInputArgumentDecider(PsychArgRequirementType isRequired, PsychError matchError)
{
	if(isRequired==kPsychArgRequired){
		if(matchError)
			PsychErrorExit(matchError); 
		else
			return(TRUE);
	}else if(isRequired==kPsychArgOptional){
		if(matchError==PsychError_invalidArg_absent)
			return(FALSE);
		else if(matchError)
			PsychErrorExit(matchError);
		else 
			return(TRUE);
	}else if(isRequired==kPsychArgAnything){
		if(!matchError)
			return(TRUE);
		else if(matchError==PsychError_invalidArg_absent)
			return(FALSE);
		else if(matchError==PsychError_invalidArg_type)
			return(FALSE);
		else if(matchError==PsychError_invalidArg_size)
			return(FALSE);
		else
			PsychErrorExit(matchError);
	}
	PsychErrorExitMsg(PsychError_internal, "Reached end of function unexpectedly");
	return(FALSE);			//make the compiler happy
}



/*

	PsychAcceptOutputArgumentDecider()
	
	This is a subroutine of Psychtoolbox functions such as PsychCopyCopyDoubleArg() which output arguments from Psychtoolbox functions 
	back to the scripting environment.  

*/
Boolean PsychAcceptOutputArgumentDecider(PsychArgRequirementType isRequired, PsychError matchError)
{

	if(isRequired==kPsychArgRequired){
		if(matchError)
			PsychErrorExit(matchError);							//the argument was required and absent so exit with an error. Or there was some other error.
		else 
			return(TRUE);										//the argument was required and present so go read it. 
	}else if(isRequired==kPsychArgOptional){
		if(!matchError)
			return(TRUE);										//the argument was optional and present so go read it.  
		else if(matchError==PsychError_invalidArg_absent)
			return(FALSE);										//the argument as optional and absent so dont' read  it. 
		else  if(matchError)
			PsychErrorExit(matchError);							//there was some other error
	}else if(isRequired==kPsychArgAnything) 
		PsychErrorExitMsg(PsychError_internal, "kPsychArgAnything argument passed to an output function.  Use kPsychArgOptional");
	else
		PsychErrorExit(PsychError_internal);
	
	PsychErrorExitMsg(PsychError_internal, "End of function reached unexpectedly");
	return(FALSE);		//make the compiler happy
}


/*
    PsychMatchDescriptors()
    
	Compare descriptors for specified and received arguments. Return a mismatch error if they are 
	incompatible, otherwise return a no error.
	
	PsychMatchDescriptors compares:
		The argument type
		The argument size
		Argument presense 
	
	PsychMatchDescripts can return any of the following values describing the relationship between an
	argument provided from the scripting environment and argument requested by a Psychtoolbox module:
		PsychError_internal					-Internal Psychtoolbox error
		PsychError_invalidArg_absent		-There was no argument provided
		PsychError_invalidArg_type			-The argument was present but not the specified type
		PsychError_invalidArg_size			-The argument was presnet and the specified type but not the specified size
		PsychError_none						-The argument matched the specified argument
		
    This function should be enhnaced to report the nature of the disagrement
*/


PsychError PsychMatchDescriptors(void)
{
	PsychArgDescriptorType *specified, *received;

	PsychGetArgDescriptor(&specified, &received);
	
	//check for various bogus conditions resulting only from Psychtoolbox bugs and issue an internal error. 
	if(specified->position != received->position)
		PsychErrorExit(PsychError_internal);
	if(specified->direction != received->direction)
		PsychErrorExit(PsychError_internal);
	
	if(specified->direction==PsychArgOut) {
		if(received->isThere==kPsychArgPresent || received->isThere==kPsychArgFixed)
			return(PsychError_none);
		else
			return(PsychError_invalidArg_absent);
	}
	if(specified->direction==PsychArgIn){
		if(received->isThere==kPsychArgAbsent)  
			return(PsychError_invalidArg_absent);
		//otherwise the argument is present and we proceed to the argument type and size checking block below 
	}

	//if we get to here it means that an input argument was supplied.  Check if it agrees in type and size with the specified arg and return 
	// an error type accordingly
	if(!(specified->type & received->type))
		return(PsychError_invalidArg_type);
	if(received->mDimMin != received->mDimMax || received->nDimMin != received->nDimMax ||  received->pDimMin != received->pDimMax)  
		PsychErrorExit(PsychError_internal);	//unnecessary mandate  
	if(received->mDimMin < specified->mDimMin)
		return(PsychError_invalidArg_size);
	if(received->nDimMin < specified->nDimMin)
		return(PsychError_invalidArg_size);
	if(specified->pDimMin != kPsychUnusedArrayDimension && received->pDimMin < specified->pDimMin)
		return(PsychError_invalidArg_size);
	if(specified->mDimMax != kPsychUnboundedArraySize && received->mDimMax > specified->mDimMax) 
		return(PsychError_invalidArg_size);
	if(specified->nDimMax != kPsychUnboundedArraySize && received->nDimMax > specified->nDimMax) 
		return(PsychError_invalidArg_size);
	if(specified->pDimMax != kPsychUnusedArrayDimension && specified->pDimMax != kPsychUnboundedArraySize && received->pDimMax > specified->pDimMax) 
		return(PsychError_invalidArg_size);
	if(received->numDims > 3)  //we don't allow matrices with more than 3 dimensions.
		return(PsychError_invalidArg_size);

	//if we get to here it means that  the block above it means 
	return(PsychError_none);	
}




PsychError PsychMatchDescriptorsOld(void)
{
	PsychArgDescriptorType *specified, *received;

	PsychGetArgDescriptor(&specified, &received);
	
	//check for various bogus conditions resulting only from Psychtoolbox bugs and issue an internal error
	if(specified->position != received->position)
		PsychErrorExit(PsychError_internal);
	if(specified->direction != received->direction)
		PsychErrorExit(PsychError_internal);
	
	switch(specified->direction) {
		case PsychArgOut:  
			switch(specified->isRequired){
				case kPsychArgRequired:
					switch(received->isThere){
						case kPsychArgPresent:		
							goto exitOk;					//both the argument is present and the return variable is named within the calling environment.
						case kPsychArgFixed:		    
							goto exitOk;					//the argument is present but a return variable is not named within the calling environment. In MATLAB this can only be the 1st return argument.
						case kPsychArgAbsent:				
							return(PsychError_invalidArg_absent);	//neither a return argument is present nor a return variable is named within the calling environment.
					}
				case kPsychArgOptional:	case kPsychArgAnything:			
					switch(received->isThere){
						case kPsychArgPresent:
							goto exitOk;
						case kPsychArgFixed:
							goto exitOk;
						case kPsychArgAbsent:
							goto exitOk;
					}
			}
			break;
		case PsychArgIn:
			switch(specified->isRequired){
				case kPsychArgRequired:
					switch(received->isThere){
						case kPsychArgPresent:
							break;						//we still need to comppare the actual type and size to specifications.
						case kPsychArgFixed:  
							PsychErrorExitMsg(PsychError_internal,"The input argument descriptor specifies a fixed argument, this property is unallowed for inputs.");
						case kPsychArgAbsent:
							return(PsychError_invalidArg_absent);
					}
					break;
				case kPsychArgOptional: case kPsychArgAnything: 
					switch(received->isThere){
						case kPsychArgPresent:
							break;						//we still need to comppare the actual type and size to specifications.
						case kPsychArgFixed:
							PsychErrorExitMsg(PsychError_internal,"The input argument descriptor secifies a fixed argument, this property is unallowed for inputs.");
			 			case kPsychArgAbsent:
							goto exitOk;				//we do NOT need to compare the actual type and size to specifications. 
					}
					break;

			}
			//if we get to here we are assured that both an input argument was specified and there is one there. In this block we compare they type and size of
			//specified and provide arguments.  For output arguments we skip over this block because those are not assigned types by the calling environment.  
			if(!(specified->type & received->type))
				return(PsychError_invalidArg_type);
			if(received->mDimMin != received->mDimMax || received->nDimMin != received->nDimMax ||  received->pDimMin != received->pDimMax)  
				PsychErrorExit(PsychError_internal);	//unnecessary mandate  
			if(received->mDimMin < specified->mDimMin)
				return(PsychError_invalidArg_size);
			if(received->nDimMin < specified->nDimMin)
				return(PsychError_invalidArg_size);
			if(received->pDimMin < specified->pDimMin)
				return(PsychError_invalidArg_size);
			if(specified->mDimMax != kPsychUnboundedArraySize && received->mDimMax > specified->mDimMax) 
				return(PsychError_invalidArg_size);
			if(specified->nDimMax != kPsychUnboundedArraySize && received->nDimMax > specified->nDimMax) 
				return(PsychError_invalidArg_size);
			if(specified->pDimMax != kPsychUnboundedArraySize && received->pDimMax > specified->pDimMax) 
				return(PsychError_invalidArg_size);
			if(received->numDims > 3)  //we don't allow matrices with more than 3 dimensions.
				return(PsychError_invalidArg_size);
			break; 
	}
		
	exitOk: 
	return(PsychError_none);	
}






//local function definitions for ScriptingGlue.cpp
//___________________________________________________________________________________________


Boolean PsychIsDefaultMat(CONSTmxArray *mat)
{
	return (PsychIsEmptyMat(mat) && !mxIsChar(mat));
}

Boolean PsychIsEmptyMat(CONSTmxArray *mat)
{
	return(mxGetM(mat)==0 || mxGetN(mat)==0);
}


//functions for project access to module call arguments (MATLAB)
//___________________________________________________________________________________________


//functions which query the number and nature of supplied arguments

/* 
	PsychGetNumInputArgs()

	-The count excludes the command argument and includes ALL arguments supplied, including 
	default arguments.  
	
	-For the time being, the only way to check if all required arguments are supplied in the 
	general case of mixed required and optional arguments is to check each individually. Probably
	the best way to to fix this is to employ a description of which are required and which optional
	and compare that against what was passed to the subfunction.
*/
int PsychGetNumInputArgs(void)
{
	if(PsychAreSubfunctionsEnabled() && !baseFunctionInvoked) //this should probably be just baseFunctionInvoked wo PsychSubfunctionEnabled.
		return(nrhsGLUE-1);
	else
		return(nrhsGLUE);
}

int PsychGetNumOutputArgs(void)
{
	return(nlhsGLUE==0 ? 1 : nlhsGLUE);
}

int PsychGetNumNamedOutputArgs(void)
{
	return(nlhsGLUE);
}

PsychError PsychCapNumInputArgs(int maxInputs)
{
    if(PsychGetNumInputArgs() > maxInputs)
            return(PsychError_extraInputArg);
    else
            return(PsychError_none);
}

PsychError PsychRequireNumInputArgs(int minInputs)
{
    if(PsychGetNumInputArgs() < minInputs)
            return(PsychError_missingInputArg);
    else
            return(PsychError_none);
    
}

PsychError PsychCapNumOutputArgs(int maxNamedOutputs)
{
	if(PsychGetNumNamedOutputArgs() > maxNamedOutputs)
		return(PsychError_extraOutputArg);
	else
		return(PsychError_none);
}



/*
	The argument is not present if a default m*n=0 matrix was supplied, '' or []	
*/
boolean PsychIsArgPresent(PsychArgDirectionType direction, int position)
{
	int numArgs;
	
	if(direction==PsychArgOut){
		return((boolean)(PsychGetNumOutputArgs()>=position));
	}else{
		if((numArgs=PsychGetNumInputArgs())>=position)
			return(!(PsychIsDefaultMat(PsychGetInArgMxPtr(position)))); //check if its default
		else
			return(FALSE);
	}
}

/*
	The argument is present if anything was supplied, including the default matrix
*/
boolean PsychIsArgReallyPresent(PsychArgDirectionType direction, int position)
{
	
	return(direction==PsychArgOut ? PsychGetNumOutputArgs()>=position : PsychGetNumInputArgs()>=position);
}



PsychArgFormatType PsychGetArgType(int position) //this is for inputs because outputs are unspecified by the calling environment.
{
	if(!(PsychIsArgReallyPresent(PsychArgIn, position)))
		return(PsychArgType_none);
	
	return(PsychGetTypeFromMxPtr(PsychGetInArgMxPtr(position)));	
}

int PsychGetArgM(int position)
{
	if(!(PsychIsArgPresent(PsychArgIn, position)))
		PsychErrorExitMsg(PsychError_invalidArgRef,NULL);
	return( mxGetM(PsychGetInArgMxPtr(position)));
}

int PsychGetArgN(int position)
{
	if(!(PsychIsArgPresent(PsychArgIn, position)))
		PsychErrorExitMsg(PsychError_invalidArgRef,NULL);
	return( mxGetNOnly(PsychGetInArgMxPtr(position)));
}


int PsychGetArgP(int position)
{
	if(!(PsychIsArgPresent(PsychArgIn, position)))
		PsychErrorExitMsg(PsychError_invalidArgRef,NULL);
	return( mxGetP(PsychGetInArgMxPtr(position)));
}


/*
    PyschCheckInputArgType()
    
    Check that the input argument at the specifid position matches at least one of the types passed in the argType
    argument.  If the argument violates the proscription exit with an error.  Otherwise return a boolean indicating
    whether the argument was present.   
    
*/
boolean PsychCheckInputArgType(int position, PsychArgRequirementType isRequired, PsychArgFormatType argType)
{
	PsychError		matchError;
	Boolean			acceptArg;

    PsychSetReceivedArgDescriptor(position, PsychArgIn);
    PsychSetSpecifiedArgDescriptor(position, PsychArgIn, argType, isRequired, 0,kPsychUnboundedArraySize,0,kPsychUnboundedArraySize,0,kPsychUnboundedArraySize);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
    return(acceptArg);
}



 
/*functions which output arguments.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
-Naming scheme:
	-Outputing return arguments:
		- "PsychAllocOut*Arg" : allocate and set a pointer to volatile memory to be filled with returned information by the caller.
		- "PsychCopyOut*Arg : accept a pointer to ouput values and fill in the return matrix memory with the values.
	-Reading input arguments:
		- "PsychAllocIn*Arg" : set a pointer to volatile memory allocated by "PsychAllocIn*Arg" and holding the input value.
		- "PsychCopyIn*Arg" : accept a pointer to memory and fill in that memory with the input argument values.     

-These all supply their own dynamic memory now, even functions which return arguments, and, in the case of
 Put functions,  even when those arguments are not present !  If you don't want the function to go allocating
 memory for an unsupplied return argument, detect the presense of that argument from within your script and
 conditionally invoke PsychPut*Arg.  This is a feature which allows you to ignore the presense of a return 
 argument in the case where memory which holds a return argument serves other purposes.   
 
-All dynamic memory provided by these functions is volatile, that is, it is lost when the mex module exits and
returns control to the Matlab environemnt.  To make it non volatile, call Psych??? on it.  

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/



boolean PsychCopyOutDoubleArg(int position, PsychArgRequirementType isRequired, double value)
{
	mxArray **mxpp;
	PsychError matchError;
	Boolean putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_double,  isRequired, 1,1,1,1,0,0);
	matchError=PsychMatchDescriptors();
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		(*mxpp) = mxCreateDoubleMatrix(1,1,mxREAL);
		mxGetPr((*mxpp))[0] = value;
	}
	return(putOut);
}



/*
    PsychAllocOutDoubleArg_2()
    
    usage:
    boolean PsychAllocOutDoubleArg_2(int position, PsychArgRequirementType isRequired, double **value)
    boolean PsychAllocOutDoubleArg_2(int position, PsychArgRequirementType isRequired, double **value, PsychGenericScriptType **nativeDouble)
    
    PsychAllocOutDoubleArg_2() is an experimental enhanced version of PsychAllocOutDoubleArg which will accept the kPsychNoArgReturn  
    constant in the position argument and then return via the optional 4th input a pointer to a native scripting type which holds the 
    double.
    
    Having a reference to the native type allows us to embed doubles withing cell arrays and structs and to pass doubles as arguments to functions
    called within MATLAB from a mex file.
    
    PsychAllocOutDoubleArg_2() should be backwards compatible with PsychAllocOutDoubleArg and could supplant that function.      
    
*/
/*
boolean PsychAllocOutDoubleArg_2(int position, PsychArgRequirementType isRequired, double **value, ...)
{
	mxArray **mxpp;
        va_list ap;
        
        if(position != kPsychNoArgReturn){
            PsychSetReceivedArgDescriptor(position, PsychArgOut);
            PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_double, isRequired, 1,1,1,1,0,0);
            PsychErrorExit(PsychMatchDescriptors());    
            mxpp = PsychGetOutArgMxPtr(position);
            if(mxpp == NULL){  //Here we allocated memory even if the return argument is not present.  Controversial.  
                    *value= (double *)mxMalloc(sizeof(double));
                    return(FALSE); 
            }
            else{
                    *mxpp = mxCreateDoubleMatrix3D(1,1,0);
                    *value = mxGetPrPtr(*mxpp);
                    return(TRUE);   
            }
        }else{
            va_start(ap, value);
            *(mxArray**)ap=mxCreateDoubleMatrix3D(1,1,0);
            *value = mxGetPrPtr(*(mxArray**)ap);
            va_end(ap);
            return(TRUE);
        }
}  
*/


  
boolean PsychAllocOutDoubleArg(int position, PsychArgRequirementType isRequired, double **value)
{
	mxArray			**mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_double, isRequired, 1,1,1,1,0,0);
	matchError=PsychMatchDescriptors();
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateDoubleMatrix3D(1,1,0);
		*value = mxGetPr(*mxpp);
	}else{
		mxpp = PsychGetOutArgMxPtr(position);
		*value= (double *)mxMalloc(sizeof(double));
	}
	return(putOut);
}


/* 
PsychAllocOutDoubleMatArg()

A)return argument mandatory:
	1)return argument not present: 		exit with an error.
	2)return argument present: 		allocate an output matrix and set return arg pointer. Set *array to the array within the new matrix. Return TRUE.  
B)return argument optional:
	1)return argument not present:  	return FALSE to indicate absent return argument.  Create an array.   Set *array to the new array. 
	2)return argument present:	 	allocate an output matrix and set return arg. pointer. Set *array to the array within the new matrix.  Return TRUE.   
*/
boolean PsychAllocOutDoubleMatArg(int position, PsychArgRequirementType isRequired, int m, int n, int p, double **array)
{
	mxArray			**mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_double, isRequired, m,m,n,n,p,p);
	matchError=PsychMatchDescriptors();
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateDoubleMatrix3D(m,n,p);
		*array = mxGetPr(*mxpp);
	}else
		*array= (double *)mxMalloc(sizeof(double)*m*n*maxInt(1,p));
	return(putOut);
}


/*
    PsychCopyOutBooleanArg()
*/
boolean PsychCopyOutBooleanArg(int position, PsychArgRequirementType isRequired, PsychNativeBooleanType value)
{
	mxArray			**mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_boolean, isRequired, 1,1,1,1,0,0);
	matchError=PsychMatchDescriptors();
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		(*mxpp) = mxCreateLogicalMatrix(1,1);
		mxGetLogicals((*mxpp))[0] = value;
	}
	return(putOut);
}



/*
    PsychAllocOutBooleanArg()
*/
boolean PsychAllocOutBooleanArg(int position, PsychArgRequirementType isRequired, PsychNativeBooleanType **value)
{
	mxArray **mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_boolean, isRequired, 1,1,1,1,0,0);
	matchError=PsychMatchDescriptors(); 
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		(*mxpp) = mxCreateLogicalMatrix(1,1);
		*value = mxGetLogicals((*mxpp));
	}else{
		mxpp = PsychGetOutArgMxPtr(position);
		*value= (PsychNativeBooleanType *)mxMalloc(sizeof(PsychNativeBooleanType));
	}
	return(putOut);
}    


/* 
    PsychAllocOutBooleanMatArg()

    A)return argument mandatory:
	1)return argument not present: 		exit with an error.
	2)return argument present: 		allocate an output matrix and set return arg pointer. Set *array to the array within the new matrix. Return TRUE.  
    B)return argument optional:
	1)return argument not present:  	return FALSE to indicate absent return argument.  Create an array.   Set *array to the new array. 
	2)return argument present:	 	allocate an output matrix and set return arg. pointer. Set *array to the array within the new matrix.  Return TRUE.   
*/
boolean PsychAllocOutBooleanMatArg(int position, PsychArgRequirementType isRequired, int m, int n, int p, PsychNativeBooleanType **array)
{
	mxArray			**mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_boolean, isRequired, m,m,n,n,p,p);
	matchError=PsychMatchDescriptors(); 
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateNativeBooleanMatrix3D(m,n,p);
		*array = (PsychNativeBooleanType *)mxGetLogicals(*mxpp);
	}else{
		*array= (PsychNativeBooleanType *)mxMalloc(sizeof(PsychNativeBooleanType)*m*n*maxInt(1,p));
	}
	return(putOut);
}




/* 
    PsychAllocOutUnsignedByteMatArg()
    
    Like PsychAllocOutDoubleMatArg() execept for unsigned bytes instead of doubles.  
*/
boolean PsychAllocOutUnsignedByteMatArg(int position, PsychArgRequirementType isRequired, int m, int n, int p, ubyte **array)
{
	mxArray **mxpp;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_uint8, isRequired, m,m,n,n,p,p);
	matchError=PsychMatchDescriptors(); 
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateByteMatrix3D(m,n,p); 
		*array = (ubyte *)mxGetPr(*mxpp);
	}else{
		*array= (ubyte *)mxMalloc(sizeof(ubyte)*m*n*maxInt(1,p));
	}
	return(putOut);
}



boolean PsychCopyOutDoubleMatArg(int position, PsychArgRequirementType isRequired, int m, int n, int p, double *fromArray)
{
	mxArray **mxpp;
	double *toArray;
	PsychError		matchError;
	Boolean			putOut;
	
	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_double, isRequired, m,m,n,n,p,p);
	matchError=PsychMatchDescriptors(); 
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateDoubleMatrix3D(m,n,p);
		toArray = mxGetPr(*mxpp);
		//copy the input array to the output array now
		memcpy(toArray, fromArray, sizeof(double)*m*n*maxInt(1,p));
	}
	return(putOut);
}


/*
	PsychCopyOutCharArg()

	Accept a null terminated string and return it in the specified position.  
	  
*/
boolean PsychCopyOutCharArg(int position, PsychArgRequirementType isRequired, const char *str)
{
	mxArray **mxpp;
	PsychError		matchError;
	Boolean			putOut;	

	PsychSetReceivedArgDescriptor(position, PsychArgOut);
	PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_char, isRequired, 0, strlen(str),0,strlen(str),0,0);
	matchError=PsychMatchDescriptors(); 
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxpp = PsychGetOutArgMxPtr(position);
		*mxpp = mxCreateString(str);
	}
	return(putOut);
}




/*functions which input arguments.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/



/*
A)input argument mandatory:
 
	1)input argument not present: 		exit with error.
	2)input argument present: 			set *array to the input matrix, *m, *n, and *p to its dimensions, return TRUE.    
B)input argument optional:

	1)input argument not present: 		return FALSE
	2)input argument present: 			set *array to the input matrix, *m, *n, and *p to its dimensions, return TRUE.    

*/
// TO DO: Needs to be updated for kPsychArgAnything
boolean PsychAllocInDoubleMatArg(int position, PsychArgRequirementType isRequired, int *m, int *n, int *p, double **array)
{
    const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;
    
    PsychSetReceivedArgDescriptor(position, PsychArgIn);
    PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_double, isRequired, 1,-1,1,-1,0,-1);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		*m = mxGetM(mxPtr);
		*n = mxGetNOnly(mxPtr);
		*p = mxGetP(mxPtr);
		*array=mxGetPr(mxPtr);
	}
	return(acceptArg);
}



/*
	PsychAllocInIntegerListArg()
	
	In a scriptiong language such as MATLAB where numbers are almost always stored as doubles, this function is useful to check
	that the value input is an integer value stored within a double type.
	
	Otherwise it just here to imitate the version written for other scripting languages.
*/
boolean PsychAllocInIntegerListArg(int position, PsychArgRequirementType isRequired, int *numElements, int **array)
{
    int m, n, p,i; 
    double *doubleMatrix;
    boolean isThere; 

    isThere=PsychAllocInDoubleMatArg(position, isRequired, &m, &n, &p, &doubleMatrix);
    if(!isThere)
        return(FALSE);
    p= (p==0) ? 1 : p;
    *numElements = m * n * p;    				
    *array=(int*) mxMalloc(*numElements * sizeof(int));
    for(i=0;i<*numElements;i++){
        if(!PsychIsIntegerInDouble(doubleMatrix+i))
            PsychErrorExit(PsychError_invalidIntegerArg);
        (*array)[i]=(int)doubleMatrix[i];
    }
    return(TRUE);    
}



/*
    PsychAllocInByteMatArg()
    
    Like PsychAllocInDoubleMatArg() except it returns an array of unsigned bytes.  
*/
boolean PsychAllocInUnsignedByteMatArg(int position, PsychArgRequirementType isRequired, int *m, int *n, int *p, unsigned char **array)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;

	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_uint8, isRequired, 1,-1,1,-1,0,-1);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		*m = (int)mxGetM(mxPtr);
		*n = (int)mxGetNOnly(mxPtr);
		*p = (int)mxGetP(mxPtr);
		*array=(unsigned char *)mxGetPr(mxPtr);
	}
	return(acceptArg);
}

			 



/* 
	PsychCopyInDoubleArg()
	
	For 1x1 double.
 
	Return in *value a double passed in the specified position, or signal an error if there is no 
	double there and the argument is required, or don't change "value" if the argument is optional
	and none is supplied.  
	
    Note that if the argument is optional and ommitted PsychGetDoubleArg won't overwrite *value, allowing 
    for specification of default values within project files without checking for their
    presense and conditinally filing in values.  
*/
// TO DO: Needs to be updated for kPsychArgAnything
boolean PsychCopyInDoubleArg(int position, PsychArgRequirementType isRequired, double *value)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;
	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_double, isRequired, 1,1,1,1,1,1);
	matchError=PsychMatchDescriptors();

	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		*value=mxGetPr(mxPtr)[0]; 
	}

	return(acceptArg); 
}



/*  
    Like PsychCopyInDoubleArg() with the additional restriction that the passed value not have a fractoinal componenet
    and that the it fit within thebounds of a C integer
    
    We could also accept matlab native integer types by specifying a conjunction of those as the third argument 
    in the PsychSetSpecifiedArgDescriptor() call, but why bother ?    
*/
boolean PsychCopyInIntegerArg(int position,  PsychArgRequirementType isRequired, int *value)
{
	const mxArray 	*mxPtr;
	double			tempDouble;
	PsychError		matchError;
	Boolean			acceptArg;

	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_double, isRequired, 1,1,1,1,1,1);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		tempDouble=mxGetPr(mxPtr)[0];
		printf("INTARG %i %lf\n", position, tempDouble); fflush(NULL);
                if(!PsychIsIntegerInDouble(&tempDouble))
                    PsychErrorExit(PsychError_invalidIntegerArg);
                *value=(int)tempDouble;
	}
	return(acceptArg);
}



/*
    PsychAllocInDoubleArg()
     
*/
boolean PsychAllocInDoubleArg(int position, PsychArgRequirementType isRequired, double **value)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;
	
	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_double, isRequired, 1,1,1,1,1,1);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		*value=mxGetPr(mxPtr);
	}
	return(acceptArg);
}



/*
	PsychAllocInCharArg()
	
	Reads in a string and sets *str to point to the string.
	
	This function violates the rule for AllocIn fuctions that if the argument is optional and absent we allocate 
	space.  That turns out to be an unuseful feature anyway, so we should probably get ride of it.

	The second argument has been modified to passively accept, without error, an argument in the specified position of non-character type.  
          
        0	kPsychArgOptional  Permit either an argument of the specified type or no argument.  An argument of any a different type is an error.
        1	kPsychArgRequired  Permit only an argument of the specifed type.  If no argument is present, exit with error.
        2	kPsychArgAnything  Permit any argument type without error, but only read the specified type. 
		
*/
boolean PsychAllocInCharArg(int position, PsychArgRequirementType isRequired, char **str)
{
	const mxArray 	*mxPtr;
	int status,strLen;	
	PsychError		matchError;
	Boolean			acceptArg;


	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, PsychArgType_char, isRequired, 0, kPsychUnboundedArraySize ,0, kPsychUnboundedArraySize, 0 , 1);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		strLen = (mxGetM(mxPtr) * mxGetNOnly(mxPtr)) + 1;
		*str = (char *)PsychCallocTemp(strLen, sizeof(char));
		status = mxGetString(mxPtr, *str, strLen); 
		if(status!=0)
			PsychErrorExitMsg(PsychError_internal, "mxGetString failed to get the string");
	}
	return(acceptArg);
}



/*
	Get a boolean flag from the specified argument position.  The matlab type can be be boolean, uint8, or
	char.  If the numerical value is equal to zero or if its empty then the flag is FALSE, otherwise the
	flag is TRUE.
	
	PsychGetFlagArg returns TRUE if the argument was present and false otherwise:
	
	A- Argument required
		1- Argument is present: load *argVal and return TRUE 
		2- Argument is absent: exit with an error
	B- Argument is optional
		1- Argument is present: load *argVal and return TRUE 
		2- Argument is absent: leave *argVal alone and return FALSE

	Note: if we modify PsychGetDoubleArg to accept all types and coerce them, then we could simplify by 
	calling that instead of doing all of this stuff...
		
*/
boolean PsychAllocInFlagArg(int position,  PsychArgRequirementType isRequired, boolean **argVal)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;

	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, (PsychArgFormatType)(PsychArgType_double|PsychArgType_char|PsychArgType_uint8|PsychArgType_boolean), 
									isRequired, 1,1,1,1,kPsychUnusedArrayDimension,kPsychUnusedArrayDimension);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		//unlike other PsychAllocIn* functions, here we allocate new memory and copy the input to it rather than simply returning a pointer into the received array.
		//That's because we want the booleans returned to the caller by PsychAllocInFlagArg() to alwyas be 8-bit booleans, yet we accept as flags either 64-bit double, char, 
		//or logical type.  Restricting to logical type would be a nuisance in the MATLAB environment and does not solve the problem because on some platforms MATLAB
		//uses for logicals 64-bit doubles and on others 8-bit booleans (check your MATLAB mex/mx header files).     
	    *argVal = (boolean *)mxMalloc(sizeof(boolean));
		mxPtr = PsychGetInArgMxPtr(position);
		if(mxIsLogical(mxPtr)){
			if(mxGetLogicals(mxPtr)[0])
				**argVal=(boolean)1;
			else
				**argVal=(boolean)0;
		}else{	
			if(mxGetPr(mxPtr)[0])
				**argVal=(boolean)1;
			else
				**argVal=(boolean)0;
		}
	}
	return(acceptArg);    //the argument was not present (and optional).	
}


boolean PsychAllocInFlagArgVector(int position,  PsychArgRequirementType isRequired, int *numElements, boolean **argVal)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;
	int				i;

	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, (PsychArgFormatType)(PsychArgType_double | PsychArgType_char | PsychArgType_uint8 | PsychArgType_boolean), 
									isRequired, 1, kPsychUnboundedArraySize, 1, kPsychUnboundedArraySize, kPsychUnusedArrayDimension, kPsychUnusedArrayDimension);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		*numElements=mxGetM(mxPtr) * mxGetN(mxPtr);
		//unlike other PsychAllocIn* functions, here we allocate new memory and copy the input to it rather than simply returning a pointer into the received array.
		//That's because we want the booleans returned to the caller by PsychAllocInFlagArgVector() to alwyas be 8-bit booleans, yet we accept as flags either 64-bit double, char, 
		//or logical type.  Restricting to logical type would be a nuisance in the MATLAB environment and does not solve the problem because on some platforms MATLAB
		//uses for logicals 64-bit doubles and on others 8-bit booleans (check your MATLAB mex/mx header files).     		
	    *argVal = (boolean *)mxMalloc(sizeof(boolean) * *numElements);
		for(i=0; i< *numElements;i++){
			if(mxIsLogical(mxPtr)){
				if(mxGetLogicals(mxPtr)[i])
					(*argVal)[i]=(boolean)1;
				else
					(*argVal)[i]=(boolean)0;
			}else{
				if(mxGetPr(mxPtr)[i])
					(*argVal)[i]=(boolean)1;
				else
					(*argVal)[i]=(boolean)0;
			}
		}
	}
	return(acceptArg);    //the argument was not present (and optional).	
}


/*
	PsychCopyInFlagArg()
*/
boolean PsychCopyInFlagArg(int position, PsychArgRequirementType isRequired, boolean *argVal)
{
	const mxArray 	*mxPtr;
	PsychError		matchError;
	Boolean			acceptArg;
	
	
	PsychSetReceivedArgDescriptor(position, PsychArgIn);
	PsychSetSpecifiedArgDescriptor(position, PsychArgIn, (PsychArgFormatType)(PsychArgType_double|PsychArgType_char|PsychArgType_uint8|PsychArgType_boolean), 
									isRequired, 1,1,1,1,kPsychUnusedArrayDimension,kPsychUnusedArrayDimension);
	matchError=PsychMatchDescriptors();
	acceptArg=PsychAcceptInputArgumentDecider(isRequired, matchError);
	if(acceptArg){
		mxPtr = PsychGetInArgMxPtr(position);
		if(mxIsLogical(mxPtr)){
			if(mxGetLogicals(mxPtr)[0])
				*argVal=(boolean)1;
			else
				*argVal=(boolean)0;
		}else{	
			if(mxGetPr(mxPtr)[0])
				*argVal=(boolean)1;
			else
				*argVal=(boolean)0;
		}	
	}
	return(acceptArg);    //the argument was not present (and optional).	
}



boolean PsychCopyOutFlagArg(int position, PsychArgRequirementType isRequired, boolean argVal)
{
	return(PsychCopyOutDoubleArg(position, isRequired, (double)argVal));
}


/*
    PsychAllocOutFlagListArg()
	
	This seems silly.  Find out where its used and consider using an array of booleans instead.  Probably the best thing
	is just to transparently map arrays of booleans to logical arrays MATLAB.  
    
    In Matlab our boolean flags are actually doubles.  This will not be so in all scripting languages.  We disguise the 
    implementation of boolean flags within the scripting envrironment by making the flag list opaque and
    providing accessor fucntions PsychLoadFlagListElement, PsychSetFlagListElement, and PsychClearFlagListElement.
    
    TO DO: maybe this should return a logical array instead of a bunch of doubles.  Itwould be better for modern versions
	of MATLAB which store doubles as bytes internally.  
	

*/
boolean PsychAllocOutFlagListArg(int position, PsychArgRequirementType isRequired, int numElements, PsychFlagListType *flagList)
{
    return(PsychAllocOutDoubleMatArg(position, isRequired, (int)1, numElements, (int)0, flagList));
}

void PsychLoadFlagListElement(int index, boolean value, PsychFlagListType flagList)
{
    flagList[index]=(double)value; 
}
  	  
void PsychSetFlagListElement(int index, PsychFlagListType flagList)
{
    flagList[index]=(double)1;
}

void PsychClearFlagListElement(int index, PsychFlagListType flagList)
{
    flagList[index]=(double)0;
}
	


// functions which allocate native types without assigning them to return arguments.
// this is useful for embedding native structures within each other. 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* 
    PsychAllocateNativeDoubleMat()
    
    Create an opaque native matrix.   Return both 
        - Its handle,  which is specified when nesting the native matrix nesting withing other native types.
        - A handle to the C array of doubles enclosed by the native type.
        
    If (*cArray != NULL) we copy m*n*p elements from cArray into the native matrix. 

*/
void 	PsychAllocateNativeDoubleMat(int m, int n, int p, double **cArray, PsychGenericScriptType **nativeElement)
{
    double *cArrayTemp;
	
    *nativeElement = mxCreateDoubleMatrix3D(m,n,p);
    cArrayTemp = mxGetPr(*nativeElement);
    if(*cArray != NULL)
        memcpy(cArrayTemp, *cArray, sizeof(double)*m*n*maxInt(1,p));
    *cArray=cArrayTemp; 

}


double PsychGetNanValue(void)
{
	return(mxGetNaN());
}




//end of Matlab only stuff.
#endif 

#if defined(__cplusplus)
//}
#endif

// end of PTBOCTAVE master-switch...
#endif
