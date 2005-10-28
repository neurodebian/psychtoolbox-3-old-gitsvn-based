function [linear,deviceE] = SettingsToLinearAcc(cal,settings)
% [linear,deviceE] = SettingsToLinearAcc(cal,settings)
%
% Convert from device setting coordinates to
% linear color space coordinates.  Uses full
% basis function measurements in doing
% conversions so that it can compensate for
% device primary spectral shifts.
%
% SettingsToLinearAcc has been renamed "SettingsToSensorAcc".  The old
% name, "SettingsToLinearAcc", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use SettingsToSensorAcc instead.
%
% See Also: PsychCal, PsychCalObsolete, SettingsToSensorAcc

% 11/12/93  dhb   Wrote it.
% 11/15/93  dhb   Added deviceE output.
% 8/4/96    dhb   Update for stuff bag routines.
% 8/21/97	  dhb	  Update for structures.
% 3/10/98   dhb	  Change nBasesOut to nPrimaryBases.
% 4/5/02    dhb   Call through new interface.
% 4/11/02   awi   Added help comment to use SettingsToSensorAcc instead.
%                 Added "See Also"


[linear,deviceE] = SettingsToSensorAcc(cal,settings);
