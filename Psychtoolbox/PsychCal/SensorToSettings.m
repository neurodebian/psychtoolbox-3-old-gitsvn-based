function [settings,badIndex] = SensorToSettings(cal,sensor)
% [settings,badIndex] = SensorToSettings(cal,sensor)
%
% Convert from sensor color space coordinates to device
% setting coordinates.
%
% This depends on the standard calibration globals.
%
% 9/26/93    dhb   Added calData argument, badIndex return.
% 4/5/02     dhb, ly  New calling convention.

primary = SensorToPrimary(cal,sensor);
[gamut,badIndex] = PrimaryToGamut(cal,primary);
settings = GamutToSettings(cal,gamut);

