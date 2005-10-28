function [settings, badIndex] = DeviceToSettings(cal,device)
% [settings, badIndex] = DeviceToSettings(cal,device)
%
% Convert from device color space coordinates to device
% setting coordinates.
%
% This depends on the standard calibration globals.
%
% DeviceToSettings has been renamed "PrimaryToSettings".  The old
% name, "DeviceToSettings", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use PrimaryToSettings instead.
%
% See Also: PsychCal, PsychCalObsolete, PrimaryToSettings

% 9/26/93    dhb   Added calData argument, badIndex return.
% 4/5/02     dhb, ly  Call through new routine.
% 4/11/02   awi   Added help comment to use PrimaryToSettings instead.
%                 Added "See Also"


[settings,badIndex] = PrimaryToSettings(cal,device);
