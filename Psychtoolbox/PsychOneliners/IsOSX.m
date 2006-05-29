function resultFlag = IsOSX

% function resultFlag = IsOSX
%
% OSX, OS9: Returns true if the operating system is Mac OS X.  Shorthand for
% streq(computer,'MAC')
%
% WIN: Does not yet exist in Windows.
% 
% See also: IsOS9, IsWin

resultFlag= streq(computer,'MAC') | ~isempty(findstr(computer, 'apple-darwin'));
