function [sensor,primaryE] = SettingsToSensorAcc(cal,settings)
% [sensor,primaryE] = SettingsToSensorAcc(cal,settings)
%
% Convert from device setting coordinates to
% sensor color space coordinates.  Uses full
% basis function measurements in doing
% conversions so that it can compensate for
% device primary spectral shifts.

% 11/12/93  dhb   Wrote it.
% 11/15/93  dhb   Added deviceE output.
% 8/4/96    dhb   Update for stuff bag routines.
% 8/21/97	  dhb	  Update for structures.
% 3/10/98   dhb	  Change nBasesOut to nPrimaryBases.
% 4/5/02    dhb, ly  Update for new calling interface.

nPrimaryBases = cal.nPrimaryBases;
if (isempty(nPrimaryBases))
	error('No nPrimaryBases field present in calibration structure');
end
settingsE = ExpandSettings(settings,nPrimaryBases);
primaryE = SettingsToPrimary(cal,settingsE);
sensor = PrimaryToSensor(cal,primaryE);
