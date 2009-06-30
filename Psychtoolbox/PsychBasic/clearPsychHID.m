% clearPsychHID -- Implementation of "clear PsychHID" for GNU/Octave.
%
% This routine does what Matlab's "clear PsychHID" would do, but
% it takes the special requirements of Octave into account.
%
% You can use this command in scripts for Matlab as well, it will
% do the right thing.

% History:
% 05/11/06 written (MK)

try
  if IsOctave
    if mislocked('PsychHID')
      PsychHID('JettisonModuleHelper');
    end;
  end;
catch
  % Nothing to do.
end

% Clear the module out of Matlab's or Octave's workspace.
clear PsychHID;
