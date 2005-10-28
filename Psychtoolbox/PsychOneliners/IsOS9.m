function resultFlag = IsOS9

% resultFlag = IsOS9
%
% OSX, OS9: Returns true if the operating system is Mac OS 9 family.  Shorthand for:
% streq(computer,'MAC2')
%
% WIN: Does not yet exist in Windows.
%
% See also: IsOSX, IsWin

resultFlag= streq(computer,'MAC2');
