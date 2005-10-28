/*
  PsychToolbox2/Source/Common/PsychStructGlue.c		
  
  AUTHORS:
  Allen.Ingling@nyu.edu		awi 
  
  PLATFORMS: All
  
  PROJECTS:
  12/31/02	awi		Screen on OS X
   

  HISTORY:
  12/31/02  awi		wrote it.  
  
  DESCRIPTION:
  
	PsychStructGlue defines abstracted functions to create structs passed 
	between the calling environment and the PsychToolbox. 
  
  TO DO:
  
  -All "PsychAllocOut*" functions should be modified to accept -1 as the position argument and 
  in that case allocate an mxArray which may be embedded within a struct.  We then have only one
  function for settings structure array fields for all types.  This requires the addition of another
  pointer argument which points to the native struct representation, we could accept that optionally
  only when we specify -1 as the argument number.  
  
  -PsychSetStructArray* functions should check that the named field is present in the struct
  and exit gracefully with an error. 
  
  -Consider changing PsychSetStructArrayStructElement() do de-allocate the inner struct which it was 
  passed.  Pass the pointer in indirectly and set it to point to the field embedded within the struct.  
  
  
*/


#include "Psych.h"


// functions for outputting structs
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/*
    PsychAllocOutStructArray()
    
    -If argument is optional we allocate the structure even if the argument is not present.  If this bothers you, 
    then check within the subfunction for the presense of a return argument before creating the struct array.  We
    allocate space regardeless of whether the argument is present because this is consistant with other "PsychAllocOut*" 
    functions which behave this way because in some situations subfunctions might derive returned results from values
    stored in an optional argument.
    
    -If position is -1 then don't attempt to return the created structure to the calling environment.  Instead just 
    allocate the structure and return it in pStruct.  This is how to create a structure which is embeded within another 
    structure using PsychSetStructArrayStructArray().  Note that we use -1 as the flag and not NULL because NULL is 0 and
    0 is reserved for future use as a reference to the subfunction name, of if none then the function name. 
    

*/
boolean PsychAllocOutStructArray(	int position, 
                                        PsychArgRequirementType isRequired, 
                                        int numElements,
                                        int numFields, 
                                        const char **fieldNames,  
                                        PsychGenericScriptType **pStruct)
{
    mxArray **mxArrayOut;
    int structArrayNumDims=2;
    int structArrayDims[2];
	PsychError matchError;
	Boolean putOut;
	
    
    structArrayDims[0]=1;
    structArrayDims[1]=numElements;
    
    if(position !=kPsychNoArgReturn){  //Return the result to both the C caller and the scripting environment.
        PsychSetReceivedArgDescriptor(position, PsychArgOut);
        PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_structArray, isRequired, 1,1,numElements,numElements,0,0);
        *pStruct = mxCreateStructArray(structArrayNumDims, structArrayDims, numFields, fieldNames);
		matchError=PsychMatchDescriptors();
		putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
		if(putOut){
			mxArrayOut = PsychGetOutArgMxPtr(position);
            *mxArrayOut=*pStruct;
		}
		return(putOut);
    }else{ //Return the result only to the C caller.  Ignore "required".    
        *pStruct = mxCreateStructArray(structArrayNumDims, structArrayDims, numFields, fieldNames);
        return(TRUE);
    }
            
}


/*
    PsychAssignOutStructArray()
    
    Accept a pointer to a struct array and Assign the struct array to be the designated return variable.
    
*/

boolean PsychAssignOutStructArray(	int position, 
                                        PsychArgRequirementType isRequired,   
                                        PsychGenericScriptType *pStruct)
{
    mxArray **mxArrayOut;
	PsychError matchError;
	Boolean putOut;
        
    PsychSetReceivedArgDescriptor(position, PsychArgOut);
    PsychSetSpecifiedArgDescriptor(position, PsychArgOut, PsychArgType_structArray, isRequired, 1,1,0,kPsychUnboundedArraySize,0,0);
	matchError=PsychMatchDescriptors();
	putOut=PsychAcceptOutputArgumentDecider(isRequired, matchError);
	if(putOut){
		mxArrayOut = PsychGetOutArgMxPtr(position);
        *mxArrayOut=pStruct;
	}
	return(putOut);
}





// functions for filling in struct elements by type 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
    PsychSetStructArrayStringElement()
    
    The variable "index", the index of the element within the struct array, is zero-indexed.  
*/
void PsychSetStructArrayStringElement(	char *fieldName,
                                        int index,
                                        char *text,
                                        PsychGenericScriptType *pStruct)
{
    int fieldNumber, numElements;
    boolean isStruct;
    mxArray *mxFieldValue;
    
    //check for bogus arguments
    numElements=mxGetM(pStruct) *mxGetN(pStruct);
    if(index>=numElements)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a structure field at an out-of-bounds index");
    fieldNumber=mxGetFieldNumber(pStruct, fieldName);
    if(fieldNumber==-1)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a non-existent structure name field");
    isStruct= mxIsStruct(pStruct);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a field within a non-existent structure.");
        
    //do stuff
    mxFieldValue=mxCreateString(text);
    mxSetField(pStruct, index, fieldName, mxFieldValue); 
    //mxDestroyArray(mxFieldValue);

}


/*
    PsychSetStructArrayDoubleElement()
    
    Note: The variable "index" is zero-indexed.
*/                                    
void PsychSetStructArrayDoubleElement(	char *fieldName,
                                        int index,
                                        double value,
                                        PsychGenericScriptType *pStruct)
{
    int fieldNumber, numElements;
    boolean isStruct;
    mxArray *mxFieldValue;

    //check for bogus arguments
    numElements=mxGetM(pStruct) *mxGetN(pStruct);
    if(index>=numElements)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a structure field at an out-of-bounds index");
    fieldNumber=mxGetFieldNumber(pStruct, fieldName);
    if(fieldNumber==-1)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a non-existent structure name field");
    isStruct= mxIsStruct(pStruct);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a field within a non-existent structure.");
        
    //do stuff
    mxFieldValue= mxCreateDoubleMatrix(1, 1, mxREAL);
    mxGetPr(mxFieldValue)[0]= value;
    mxSetField(pStruct, index, fieldName, mxFieldValue); 
    //mxDestroyArray(mxFieldValue);
    
}


/*
    PsychSetStructArrayBooleanElement()
    
    Note: The variable "index" is zero-indexed.
*/                                    
void PsychSetStructArrayBooleanElement(	char *fieldName,
                                        int index,
                                        boolean state,
                                        PsychGenericScriptType *pStruct)
{
    int fieldNumber, numElements;
    boolean isStruct;
    mxArray *mxFieldValue;

    //check for bogus arguments
    numElements=mxGetM(pStruct) *mxGetN(pStruct);
    if(index>=numElements)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a structure field at an out-of-bounds index");
    fieldNumber=mxGetFieldNumber(pStruct, fieldName);
    if(fieldNumber==-1)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a non-existent structure name field");
    isStruct= mxIsStruct(pStruct);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a field within a non-existent structure.");
        
    //do stuff
    mxFieldValue=mxCreateLogicalMatrix(1, 1);
    mxGetLogicals(mxFieldValue)[0]= state;
    mxSetField(pStruct, index, fieldName, mxFieldValue); 
    //mxDestroyArray(mxFieldValue);
    
}



/*
    PsychSetStructArrayStructElement()
    
    
*/
void PsychSetStructArrayStructElement(	char *fieldName,
                                        int index,
                                        PsychGenericScriptType *pStructInner,
                                        PsychGenericScriptType *pStructOuter)
{
    int fieldNumber, numElements;
    boolean isStruct;
    
    //check for bogus arguments
    numElements=mxGetM(pStructOuter) *mxGetN(pStructOuter);
    if(index>=numElements)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a structure field at an out-of-bounds index");
    fieldNumber=mxGetFieldNumber(pStructOuter, fieldName);
    if(fieldNumber==-1)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a non-existent structure name field");
    isStruct= mxIsStruct(pStructInner);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a struct field to a non-existent structure.");
    isStruct= mxIsStruct(pStructOuter);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a field within a non-existent structure.");
        
    //do stuff
    mxSetField(pStructOuter, index, fieldName, pStructInner); 
    
}



/*
    PsychSetStructArrayNativeElement()
    
    
*/
void PsychSetStructArrayNativeElement(	char *fieldName,
                                        int index,
                                        PsychGenericScriptType *pNativeElement,
                                        PsychGenericScriptType *pStructArray)
{
    int fieldNumber, numElements;
    boolean isStruct;
    
    //check for bogus arguments
    numElements=mxGetM(pStructArray) *mxGetN(pStructArray);
    if(index>=numElements)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a structure field at an out-of-bounds index");
    fieldNumber=mxGetFieldNumber(pStructArray, fieldName);
    if(fieldNumber==-1)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a non-existent structure name field");
    isStruct= mxIsStruct(pStructArray);
    if(!isStruct)
        PsychErrorExitMsg(PsychError_internal, "Attempt to set a field within a non-existent structure.");
        
    //do stuff
    mxSetField(pStructArray, index, fieldName, pNativeElement); 
    
}


                                    





