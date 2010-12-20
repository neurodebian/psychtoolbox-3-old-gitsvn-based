% clearGetSecs -- Implementation of "clear GetSecs" for GNU/Octave.
%
% This routine does what Matlab's "clear GetSecs" would do, but
% it takes the special requirements of Octave into account.
%
% You can use this command in scripts for Matlab as well, it will
% do the right thing.

% History:
% 05/11/06 written (MK)

try
  if IsOctave
    if mislocked('GetSecs')
      GetSecs('JettisonModuleHelper');
    end;
  end;
catch
  % Nothing to do.
end

% Clear the module out of Matlab's or Octave's workspace.
clear GetSecs;
