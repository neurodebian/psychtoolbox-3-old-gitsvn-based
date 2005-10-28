function [settings,badIndex] = PolarToSettings(cData,pol)
% [settings,badIndex] = PolarToSettings(cData,polar)
% 
% Compute from linear polar color space coordinates to
% device setting coordinates.
%
% PolarToSettings is obsolete.  Instead use a combination 
% of PolarToSensor and SensorToSettings to achive the same
% result.  For example, instead of:
%
% [settings,badIndex] = PolarToSettings(cData,polar);
%
% use:
%
% linear = PolarToSensor(cData,pol);
% [settings,badIndex] = SensorToSettings(cData,linear);
%
% See Also: PsychCal, PsychCalObsolete, PolarToSensor, SensorToSettings

%
% Polar coordinates are defined as radius, azimuth, and elevation.
%
% 9/26/93    dhb   Added calData argument, badIndex return.
% 2/6/94     jms   Changed 'polar' to 'pol'
% 4/11/02    awi   Added help comment to use PolarToSensor+SensorToSettings instead.
%                 Added "See Also"


% From polar to linear
linear = PolarToLinear(cData,pol);

% Call through the calibration routines
[settings,badIndex] = LinearToSettings(cData,linear);
