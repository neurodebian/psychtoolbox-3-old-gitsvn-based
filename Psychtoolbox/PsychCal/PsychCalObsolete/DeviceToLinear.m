function linear = DeviceToLinear(cal,device)
% lineara = DeviceToLinear(cal,device)
%
% Convert from linear device coordinates to linear color 
% space coordinates.  The ambient lighting is added to
% the color space coordinates of the device.
%
% This depends on the standard calibration globals.
%
% DeviceToLinear has been renamed "PrimaryToSensor".  The old
% name, "DeviceToLinear", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use PrimaryToSensor instead.
%
% See Also: PsychCal, PsychCalObsolete, PrimaryToSensor

%
% 9/26/93    dhb   Added calData argument.
% 10/19/93   dhb   Allow device characterization dimensions to exceed
%                    device settings dimensions.
% 11/11/93   dhb   Update for new calData routines.
% 8/4/96     dhb   Update for new stuff bag routines.
% 8/21/97	   dhb   Convert for structures.
% 4/5/02     dhb   Call through new named version.
% 4/20/02		 awi   Fixed typo bug where return argument variable did not
%									 have the same name as the result computed within the function. 
% 4/11/02   awi   Added help comment to use PrimaryToSensor instead.
%                 Added "See Also"

linear = PrimaryToSensor(cal,device);
