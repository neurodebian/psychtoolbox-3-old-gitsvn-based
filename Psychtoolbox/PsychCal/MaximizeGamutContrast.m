function [s,sPos,sNeg] = MaximizeGamutContrast(dir,white)
% [s,sPos,sNeg] = MaximizeGamutContrast(dir,white)
% 
% Find the scalar that maximizes the contrast around
% the white point.  Input should be specified in device
% primary coordinates.
% 
%	2/22/94		dhb		Wrote it.
% 4/3/94		dhb		Analytic version.
% 4/4/94		dhb		Avoid divide by zero problem.
% 8/17/94		dhb		Handle small values of useWhite.
% 8/23/94		dhb		Fix 8/17 fix to handle negative numbers.  Sigh.
% 2/16/98		dgp		Renamed from MaxGmtCon to MaximizeGamutContrast.
% 12/3/98   dhb   Add directional return values.
%           dhb   Remove redundant calculations.
%	9/7/00		mpr		Made zero handling code more compact; did away with unnecessary variable.

% Expand out dir to handle all inputs together.
n = size(dir,2);
useWhite = white*ones(1,n);

% Set any dir entry that are zero to something very small
% This avoids a printed divide by zero error.
verySmall = 1e-15;
index = find(abs(dir) < verySmall);
dir(index) = verySmall*ones(size(index)).*sign(dir(index));
dir(find(~dir)) = verySmall*ones(size(find(~dir)));

% Deal with small values of useWhite as well
index = find(abs(useWhite) <= verySmall);
useWhite(index) = 2*verySmall*ones(size(index)).*sign(useWhite(index));
useWhite(find(~useWhite)) = 2*verySmall*ones(size(find(~useWhite)));

% Find all of the candidate s values
s1 = (1-useWhite) ./ dir;		% white + s1*dir = 1
s2 = (-useWhite) ./ dir;		% white + s2*dir = 0	

% Get all of the s values together
allS = [s1 ; s2];
indexPos = find(allS >= 0);
indexNeg = find(allS < 0);

% Return the minimum absolute value
s = min(abs(allS));
sPos = min(abs(allS(indexPos)));
sNeg = min(abs(allS(indexNeg)));
