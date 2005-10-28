function [finalSettings,badIndex,quantized,perError,settings] = LinearToSettingsAcc(cal,linear)
% [finalSettings,badIndex,quantized,perError,settings] = LinearToSettingsAcc(cal,linear)
%
% Convert from linear color space coordinates to device
% setting coordinates.  This routine makes use of the
% full basis function information to compensate for spectral
% shifts in the device primaries with input settings.
%
% This depends on the standard calibration globals.
%
% LinearToSettingsAcc has been renamed "SensorToSettingsAcc".  The old
% name, "LinearToSettingsAcc", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use SensorToSettingsAcc instead.
%
% See Also: PsychCal, PsychCalObsolete, SensorToSettingsAcc


% 11/12/93   dhb      Wrote it.
% 3/30/94	   dhb, jms Fixed logic bug in error computation.
%					              Return finalSettings as best during iteration
% 8/4/96     dhb      Update for stuff bag routines.
% 8/21/97    dhb      Update for structures.
% 3/10/98	   dhb      Change nBasesOut to nPrimaryBases.
% 4/5/02     dhb, ly  Call through new interface.
% 4/11/02    awi   Added help comment to use SensorToSettingsAcc instead.
%                  Added "See Also"


[finalSettings,badIndex,quantized,perError,settings] = SensorToSettingsAcc(cal,linear)
