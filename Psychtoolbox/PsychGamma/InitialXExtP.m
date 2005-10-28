function x = InitialXExtP(xp)
% x = InitialXExtP(xp)
%
% Initial values for extended power function fit.
%
% If argument is passed, it is assumed to be the
% parameters for best ordinary power function fit.
%
% 8/7/00  dhb  Modify for new equation.

x = [1 0]';
if (nargin == 1)
  x(2) = xp;
end

