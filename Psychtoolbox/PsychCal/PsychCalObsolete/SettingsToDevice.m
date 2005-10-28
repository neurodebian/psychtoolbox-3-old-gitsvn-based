function [device] = SettingsToDevice(cal,settings)
% [device] = SettingsToDevice(cal,settings)
% 
% Convert from device settings coordinates to
% linear device coordinates by inverting
% the gamma correction.
%
% INPUTS:
%   calibration globals
%   settings -- column vectors in device settings
%
% SettingsToDevice has been renamed "SettingsToPrimary".  The old
% name, "SettingsToDevice", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use SettingsToPrimary instead.
%
% See Also: PsychCal, PsychCalObsolete, SettingsToPrimary

% 9/26/93    dhb   Added calData argument.
% 10/19/93   dhb   Allow gamma table dimensions to exceed device settings.
% 11/11/93   dhb   Update for new calData routines.
% 8/4/96     dhb   Update for stuff bag routines.
% 8/21/97    dhb   Update for structure.
% 4/5/02     dhb, ly  Call through new interface.
% 4/11/02    awi   Added help comment to use SettingsToPrimary instead.
%                  Added "See Also"


[device] = SettingsToPrimary(cal,settings);
