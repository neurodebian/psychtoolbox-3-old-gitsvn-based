function [device] = LinearToDevice(cal,linear)
% [device] = LinearToDevice(cal,linear)
%
% Convert from linear color space coordinates to linear device
% coordinates.
%
% This depends on the standard calibration globals.
%
% LinearToDevice has been renamed "SensorToPrimary".  The old
% name, "LinearToDevice", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use SensorToPrimary instead.
%
% See Also: PsychCal, PsychCalObsolete, SensorToPrimary

% 9/26/93    dhb   Added calData argument.
% 10/19/93   dhb   Allow device characterization dimensions to exceed
%                  linear settings dimensions.
% 11/11/93   dhb   Update for new calData routines.
% 11/17/93   dhb   Newer calData routines.
% 8/4/96     dhb   Update for stuff bag routines.
% 8/21/97    dhb   Update for structures.
% 4/5/02     dhb, ly  Call through new interface.
% 4/11/02   awi   Added help comment to use SensorToPrimary instead.
%                 Added "See Also"

device = SensorToPrimary(cal,linear);
