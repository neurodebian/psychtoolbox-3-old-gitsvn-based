/*
	PsychToolbox3/Source/Common/Screen/PsychWindowSupport.h
	
	PLATFORMS:	
	
		All.  
				
	AUTHORS:
	
		Allen Ingling		awi		Allen.Ingling@nyu.edu
		Mario Kleiner           mk              mario.kleiner at tuebingen.mpg.de

	HISTORY:
	
		12/20/02		awi		Wrote it mostly by modifying SDL-specific refugees (from an experimental SDL-based Psychtoolbox).
		11/16/04		awi		Added description.  
		04/03/05                mk              Support for stereo display output and enhanced Screen('Flip') behaviour.
                05/09/05                mk              New function PsychGetMonitorRefreshInterval -- queries (and measures) monitor refresh.
		12/27/05                mk              PsychWindowSupport.h/c contains the shared parts of the windows implementation for all OS'es.
	
        DESCRIPTION:
	
	NOTES:
	
	TO DO: 
	
*/

//include once
#ifndef PSYCH_IS_INCLUDED_PsychWindowSupport
#define PSYCH_IS_INCLUDED_PsychWindowSupport

#include "Screen.h"

psych_bool PsychOpenOnscreenWindow(PsychScreenSettingsType *screenSettings, PsychWindowRecordType **windowRecord, int numBuffers, int stereomode, double* rect, int multiSample, PsychWindowRecordType* sharedContextWindow, int specialFlags);
psych_bool PsychOpenOffscreenWindow(double *rect, int depth, PsychWindowRecordType **windowRecord);
void	PsychCloseOnscreenWindow(PsychWindowRecordType *windowRecord);
void	PsychCloseWindow(PsychWindowRecordType *windowRecord);
void	PsychCloseOffscreenWindow(PsychWindowRecordType *windowRecord);
void	PsychFlushGL(PsychWindowRecordType *windowRecord);
double	PsychFlipWindowBuffers(PsychWindowRecordType *windowRecord, int multiflip, int vbl_synclevel, int dont_clear, double flipwhen, int* beamPosAtFlip, double* miss_estimate, double* time_at_flipend, double* time_at_onset);
void	PsychSetGLContext(PsychWindowRecordType *windowRecord);
void	PsychUnsetGLContext(void);
double  PsychGetMonitorRefreshInterval(PsychWindowRecordType *windowRecord, int* numSamples, double* maxsecs, double* stddev, double intervalHint);
void    PsychVisualBell(PsychWindowRecordType *windowRecord, double duration, int belltype);
void    PsychPreFlipOperations(PsychWindowRecordType *windowRecord, int clearmode);
void    PsychPostFlipOperations(PsychWindowRecordType *windowRecord, int clearmode);
PsychWindowRecordType* PsychGetDrawingTarget(void);
void    PsychSetDrawingTarget(PsychWindowRecordType *windowRecord);
void    PsychSetupView(PsychWindowRecordType *windowRecord, psych_bool useRawFramebufferSize);
void    PsychSetupClientRect(PsychWindowRecordType *windowRecord);
void	PsychSetUserspaceGLFlag(psych_bool inuserspace);
psych_bool PsychIsUserspaceRendering(void);
double	PsychGetWhiteValueFromWindow(PsychWindowRecordType *windowRecord);
void	PsychSwitchFixedFunctionStereoDrawbuffer(PsychWindowRecordType *windowRecord);
int	PsychRessourceCheckAndReminder(psych_bool displayMessage);
psych_bool	PsychFlipWindowBuffersIndirect(PsychWindowRecordType *windowRecord);
void	PsychReleaseFlipInfoStruct(PsychWindowRecordType *windowRecord);
int	PsychSetShader(PsychWindowRecordType *windowRecord, int shader);
void	PsychDetectAndAssignGfxCapabilities(PsychWindowRecordType *windowRecord);
void	PsychExecuteBufferSwapPrefix(PsychWindowRecordType *windowRecord);
int	PsychGetCurrentShader(PsychWindowRecordType *windowRecord);
void	PsychBackupFramebufferToBackingTexture(PsychWindowRecordType *backupRendertarget);
int	PsychFindFreeSwapGroupId(int maxGroupId);
unsigned int PsychGetNrAsyncFlipsActive(void);
psych_bool PsychIsMasterThread(void);

//end include once
#endif
