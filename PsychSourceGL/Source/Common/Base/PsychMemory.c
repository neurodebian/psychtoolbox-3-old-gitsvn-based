/*
  Psychtoolbox2/Source/Common/PsychMemory.c
  
  AUTHORS:
  Allen.Ingling@nyu.edu		awi 
  
  PLATFORMS: All 
  
  PROJECTS:
  09/04/02	awi		Screen on Mac OSX
   

  HISTORY:
  09/04/02  awi		Wrote it.
  
  DESCRIPTION:


  TO DO: 
  
  		See if we even need this for any reason.  It may be vestigial.  
	  

*/

#include "Psych.h"

#if PSYCH_LANGUAGE == PSYCH_MATLAB

// If running on Matlab, we use Matlab's memory manager...
void *PsychCallocTemp(unsigned long n, unsigned long size)
{
  void *ret;
  
  if(NULL==(ret=mxCalloc((size_t)n, (size_t)size))){
    if(size * n != 0)
      PsychErrorExitMsg(PsychError_outofMemory, NULL);
  }
  return(ret);
}

void *PsychMallocTemp(unsigned long n)
{
  void *ret;
  
  if(NULL==(ret=mxMalloc((size_t)n))){
    if(n!=0)
      PsychErrorExitMsg(PsychError_outofMemory,NULL);
  }
  return(ret);
}

#else

// If not running on Matlab, we use our own allocator...

#define PTBTEMPMEMDEC(n) totalTempMemAllocated -=(n)

// Enqueues a new record into our linked list of temp. memory buffers.
// Returns the memory pointer to be passed to rest of Psychtoolbox.
void* PsychEnqueueTempMemory(void* p, unsigned long n)
{
  // Add current buffer-head ptr as next-pointer to our new buffer:
  *((unsigned int*) p) = (unsigned int) PsychTempMemHead;

  // Set our buffer as new head of list:
  PsychTempMemHead = p;

  // Add allocated buffer size as 2nd element:
  p = p + sizeof(PsychTempMemHead);
  *((unsigned long*) p) = n;

  // Accounting:
  totalTempMemAllocated += n;
  // printf("TEMPMALLOCED %i Bytes,  new = %i.\n", n, totalTempMemAllocated); fflush(NULL);

  // Increment p again to get real start of user-visible buffer:
  p = p + sizeof(n);

  // Return ptr:
  return(p);
}

void *PsychCallocTemp(unsigned long n, unsigned long size)
{
  void *ret;
  // MK: This could create an overflow if product n * size is
  // bigger than length of a unsigned long int --> Only
  // happens if more than 4 GB of RAM are allocated at once.
  // --> Improbable for PTB, unless someones trying a buffer
  // overflow attack -- PTB would lose there badly anyway...
  unsigned long realsize = n * size + sizeof(void*) + sizeof(realsize);

  // realsize has extra bytes allocated for our little header...  
  if(NULL==(ret=calloc((size_t) 1, (size_t) realsize))) {
    PsychErrorExitMsg(PsychError_outofMemory, NULL);
  }

  // Need to enqueue memory buffer...
  return(PsychEnqueueTempMemory(ret, realsize));
}

void *PsychMallocTemp(unsigned long n)
{
  void *ret;

  // Allocate some extra bytes for our little header...
  n=n + sizeof(void*) + sizeof(n);
  if(NULL==(ret=malloc((size_t) n))){
    PsychErrorExitMsg(PsychError_outofMemory,NULL);
  }

  // Need to enqueue memory buffer...
  return(PsychEnqueueTempMemory(ret, n));
}

// Free a single spec'd temp memory buffer.
// TODO Note: The current implementation of our allocator
// uses a single-linked list, which has O(1) cost for
// allocating memory (Optimal!) and O(n) cost for freeing
// all allocated memory (Optimal!), but it has up to
// O(n) cost for deleting a single memory buffer as well,
// be n the total number of allocated buffers. This is
// worst-case upper bound. If PsychFreeTemp() is used a
// lot on long buffer lists, this will incur significant
// overhead! A better implementation would use a double-
// linked list or even a binary tree or hash structure,
// but for now this has to be good enough(TM).
void PsychFreeTemp(void* ptr)
{
  void* ptrbackup = ptr;
  unsigned long* psize = NULL;
  unsigned int* next = PsychTempMemHead;
  unsigned int* prevptr = NULL;

  if (ptr == NULL) return;
 
  // Convert ptb supplied pointer ptr into real start
  // of our buffer, including our header:
  ptr = ptr - sizeof(ptr) - sizeof(unsigned long);
  if (ptr == NULL) return;

  if (PsychTempMemHead == ptr) {
    // Special case: ptr is first buffer in queue. Dequeue:
    PsychTempMemHead = (unsigned int*) *PsychTempMemHead;

    // Some accounting:
    PTBTEMPMEMDEC(((unsigned int*)ptr)[1]);

    // Release it:
    free(ptr);

    return;
  }

  // ptr valid and not first buffer in queue.
  // Walk the whole buffer list until we encounter our buffer:
  while (next != NULL && next!=ptr) {
    prevptr = next;
    next = (unsigned int*) *next;
  }

  // Done with search loop. Did we find our buffer?
  if (next == ptr) {
    // Found it! Set next-ptr of previous buffer to next-ptr
    // of this buffer to dequeue from list:
    *prevptr = *next;

    // Some accounting:
    PTBTEMPMEMDEC(next[1]);
    
    // Release:
    free(ptr);

    // Done.
    return;
  }

  // Oops.: Did not find matching buffer to pointer --> Trouble!
  printf("PTB-BUG: In PsychFreeTemp: Tried to free non-existent temporary membuffer %p!!! Ignored.\n", ptrbackup);
  fflush(NULL);
  return;
}

// Master cleanup routine: Frees all allocated memory:
void PsychFreeAllTempMemory(void)
{
  unsigned int* p = NULL;
  unsigned long* psize = NULL;
  unsigned int* next = PsychTempMemHead;

  // Walk our whole buffer list and release all buffers on it:
  while (next != NULL) {
    // next points to current buffer to release. Make a copy of
    // next:
    p = next;

    // Update next to point to the next buffer to release:
    next = (unsigned int*) *p;

    // Some accounting:
    PTBTEMPMEMDEC(p[1]);

    // Release buffer p:
    free(p);
    
    // We're done with this buffer, next points to next one to release
    // or is NULL if all released...
  }

  // Done. NULL-out the list start ptr:
  PsychTempMemHead = NULL;

  // Sanity check:
  if (totalTempMemAllocated != 0) {
    // Cannot use PsychErrorXXX Routines here, because this is outside
    // the jumpbuffer context for our error-routines. Could lead to
    // infinite recursion!!!
    printf("PTB-CRITICAL BUG: Inconsistency detected in temporary memory allocator!\n");
    printf("PTB-CRITICAL BUG: totalTempMemAllocated = %i after PsychFreeAllTempMemory()!!!!\n",
	   totalTempMemAllocated);
    fflush(NULL);

    // Reset to defined state.
    totalTempMemAllocated = 0;
  }

  return;
}

#endif

