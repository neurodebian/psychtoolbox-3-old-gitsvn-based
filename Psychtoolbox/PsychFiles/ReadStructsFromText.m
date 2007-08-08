function theStructs = ReadStructsFromText(filename)
% theStructs = ReadStructsFromText(filename)
%
% Open a tab delimited text file.  The first row should
% contain the field names for a structure.  Each following
% row contains the data for one instance of that structure.
%
% This routine reads each row and returns an array of structures,
% one struct for each row, with the data filled in.  Data
% can be numeric or string for each field.
%
% Not a lot of checking is done for cases where the read file
% fails to conform to the necessary format.
%
% See Also: WriteStructsToText

% 6/15/03   dhb			Wrote it.
% 07/01/03  dhb 		Support string as well as numeric data.
% 07/02/03	dhb, jg     Handle white space in column headers.
% 07/03/03  dhb         More little tweaks.
% 08/06/03  dhb         Handle fgetl returns empty string.
% 08/22/07  dhb         This was modified on disk but not commented our uploaded to SVN repository.

% Open the file
fid = fopen(filename);
if (fid == -1)
	error('Cannot open file %s', filename);
end

% Read first line to get field names for returned structure
theFields = {};
firstLine = fgetl(fid);
theIndex = 1;
i = 1;
while (1)
	wholeField = [];
	while (1)
		readString = firstLine(theIndex:end);
		[field,count,nil,nextIndex] = sscanf(readString,'%s',1);
		if (count == 0)
			break;
		end
		wholeField = [wholeField field];
		theIndex = theIndex+nextIndex-1;
		if (nextIndex <= length(readString) & abs(readString(nextIndex)) == 9)
			break;
		else
			wholeField = [wholeField ' '];
		end
	end
	if (count == 0)
		if (~isempty(wholeField))
			theFields{i} = wholeField;
			i = i+1;
		end
		break;
	end
	theFields{i} = wholeField;
	wholeField = [];
	i = i+1;
end
nFields = length(theFields);

% Squeeze white space out of each field
for i = 1:nFields
	newField = [];
	oldField = theFields{i};
	for j = 1:length(oldField)
		if (~isspace(oldField(j)) & oldField(j) ~= '.')
			newField = [newField oldField(j)];
		end
	end
	theFields{i} = newField;
end

% Now read lines and pull out structure elements
f = 1;
while (1)
	theLine = fgetl(fid);
	if (isempty(theLine) | theLine == -1)
		break;
	end
	theIndex = 1;
	theData = cell(nFields,1);
	for i = 1:nFields
		readString = theLine(theIndex:end);
		[field,count,nil,nextIndex] = sscanf(readString,'%g',1);
		if (count == 0)
			[field,count,nil,nextIndex] = sscanf(readString,'%s',1);
			if (count == 0)
				error('Cannot parse input');
			end
		end
		theIndex = theIndex+nextIndex-1;
		theData{i} = field;
	end
	theStruct = cell2struct(theData,theFields,1);
	theStructs(f) = theStruct;
	f = f+1;
end

% Close the file.
fclose(fid);
