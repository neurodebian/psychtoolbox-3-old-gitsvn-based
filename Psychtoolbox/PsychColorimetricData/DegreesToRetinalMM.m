function mm = DegreesToRetinalMM(degs,eyeLengthMM)
%  mm = DegreesToRetinalMM(degs,eyeLengthMM)
%
% Convert extent in degrees to mm of retina in the fovea.
%
% See also: RetinalMMToDegrees, EyeLength.
%
% 7/15/03  dhb  Wrote it.

mm = 2*tan((pi/180)*degs/2)*eyeLengthMM;
