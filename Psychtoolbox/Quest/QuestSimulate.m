function response=QuestSimulate(q,tTest,tActual)
% response=QuestSimulate(q,intensity,tActual)
%
% Simulate the response of an observer with threshold tActual.
% 
% See Quest.

% Denis Pelli, 6/8/96
% 3/1/97 dgp restrict intensity parameter to range of x2.
% 3/1/97 dgp updated to use Matlab 5 structs.
% 4/12/99 dgp dropped support for Matlab 4.

% Copyright (c) 1996-2004 Denis Pelli

if nargin~=3
	error('Usage: response=QuestSimulate(q,tTest,tActual)')
end
if length(q)>1
	error('can''t deal with q being a vector')
end
t=min(max(tTest-tActual,q.x2(1)),q.x2(end));
response=interp1(q.x2,q.p2,t) > rand(1);
