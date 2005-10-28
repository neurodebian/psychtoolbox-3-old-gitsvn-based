function linear = CylToLinear(cal,cyl)
% linear = CylToLinear(cal,cyl)
%
% Convert from linear to cylindrical coordinates.
% We use the conventions of the CIE Lxx color spaces
% for angle
%
% CylToLinear has been renamed "CylToSensor".  The old
% name, "CylToLinear", is still provided for compatability 
% with existing scripts but will disappear from future releases 
% of the Psychtoolbox.  Please use CylToSensor instead.
%
% See Also: PsychCal, PsychCalObsolete, CylToSensor

% 10/17/93    dhb   Wrote it by converting CAP C code.
% 2/20/94     jms   Added single argument case to allow avoiding cData
% 4/5/02      dhb, ly  Call through new name.
% 4/11/02   awi   Added help comment to use CylToSensor instead.
%                 Added "See Also"

if (nargin == 1)
	linear = CylToSensor(cyl);
else
	linear = CylToSensor(cal,cyl);
end

