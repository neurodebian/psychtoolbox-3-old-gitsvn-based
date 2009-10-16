function trial=QuestTrials(q,binsize)
% trial=QuestTrials(q,[binsize])
% 
% Return sorted list of intensities and response frequencies.
% "binsize", if supplied, will be used to round intensities to nearest multiple of binsize.
% Here's how you might use this function to display your results:
% 		t=QuestTrials(q,0.1);
% 		fprintf(' intensity     p fit         p    trials\n');
% 		disp([t.intensity; QuestP(q,t.intensity-logC); (t.responses(2,:)./sum(t.responses)); sum(t.responses)]');
% 
% See Quest.

% Denis Pelli
% 5/4/99 dgp wrote it
% 10/13/04 dgp Added optional binsize argument
% 10/13/04 dgp The orientations of the returned vectors was inconsistent. They are now rows.
% Copyright (c) 1996-1999 Denis Pelli
% 10/16/09 mk  Bugfixes & Improvements to input argument checking, as proposed by Todd Horowitz.

if nargin < 1
	error('Usage: trial=QuestTrials(q,[binsize])')
end

if nargin < 2
    binsize = [];
end

if isempty(binsize) | ~isfinite(binsize)  %#ok<OR2>
	binsize=0;
end

if binsize < 0
	error('binsize cannot be negative')
end

if length(q)>1
	for i=1:length(q(:))
		trial(i)=QuestTrials(q(i)); %#ok<AGROW>
	end
	return
end

% sort
[intensity,i]=sort(q.intensity);
response(1:length(i))=q.response(i);

% quantize
if binsize>0
	intensity=round(intensity/binsize)*binsize;
end

% compact
j=1;
trial.intensity(1,j)=intensity(1);
trial.responses(1:2,j)=[0 0];
for i=1:length(intensity)
	if intensity(i)~=trial.intensity(j)
		j=j+1;
		trial.intensity(1,j)=intensity(i);
		trial.responses(1:2,j)=[0 0];
	end
	trial.responses(response(i)+1,j)=trial.responses(response(i)+1,j)+1;
end
