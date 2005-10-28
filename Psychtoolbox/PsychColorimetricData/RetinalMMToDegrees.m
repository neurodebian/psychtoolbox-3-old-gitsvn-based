function degs = RetinalMMToDegrees(mm,eyeLengthMM)
% degs = RetinalMMToDegrees(mm,eyeLengthMM)
%
% Convert extent in mm of retina in the fovea to degrees
% of visual angle.
%
% See also: DegreesToRetinalMM, EyeLength.
%
% 7/15/03  dhb  Wrote it.

tanarg = (mm/2)/eyeLengthMM;
degs = 2*(180/pi)*atan(tanarg);
