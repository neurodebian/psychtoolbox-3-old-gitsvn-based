function cal = SetGammaMethod(cal,gammaMode,precision)
% cal = SetGammaMethod(cal,gammaMode,[precision])
%
% Set up the gamma correction mode to be used.  Options
% are:
%   gammaMode == 0 - exhaustive table search.  Accurate but slow.
%   gammaMode == 1 - inverse table lookup.  Fast but less accurate.
%
% If gammaMode == 1, then you may specify the precision of the
% inverse table.  The default is 1000 levels.
%
% 8/4/96  dhb  Wrote it.
% 8/21/97 dhb  Update for structure.
% 3/12/98 dhb  Change name to SetGammaCorrectMode

% Check that the needed data is available. 
gammaTable = cal.gammaTable;
gammaInput = cal.gammaInput;
if isempty(gammaTable)
	error('Calibration structure does not contain gamma data');
end

% Do the right thing depending on mode.
if gammaMode == 0
	cal.gammaMode = gammaMode;
	return;
elseif gammaMode == 1
	if nargin == 2
		precision = 1000;
	end
	iGammaTable = InvertGammaTable(gammaInput,gammaTable,precision);
	cal.gammaMode = gammaMode;
	cal.iGammaTable = iGammaTable;
else
  error('Requested gamma inversion mode %g is not yet implemented', gammaMode);
end
	
