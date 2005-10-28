function directory=CalDataFolder(forceDemo)
% directory=CalDataFolder([forceDemo])
%
% If alt is true (false by default), then force use of
% PsychCalDemoData.
%
% Get the path to the CalData folder.

% Denis Pelli 7/25/96
% Denis Pelli 2/28/98 change "CalDat" to "PsychCalData"
% 8/14/00  dhb  Add alternate name, change names.

% Set forceDemo flag
if (nargin < 1 | isempty(forceDemo))
	forceDemo = 0;
end

name='PsychCalLocalData';
alternateName ='PsychCalDemoData';

% Find name.  If not there, find alternate
if (~forceDemo)
	directory = FindFolder(name);
else
	directory = [];
end
if isempty(directory)
	directory=FindFolder(alternateName);
end

% If both finds fail, print out error message.  This
% should never happen as we put 'PsychCalDemoData' in
% the toolbox distribution.
if isempty(directory)
	error(['Can''t find any ''' name ''' or ''' alternateName '''folders in the Matlab path.']);
end

% If we found multiple copies of a calibration folder, we complain.
% This also should never happen.
if size(directory,1)>1
	for i=1:size(directory,1)
		disp(['DUPLICATE: ''' deblank(directory(i,:)) '''']);
	end
	error(['Found more than one ''' name ''' folder in the Matlab path.']);
end
