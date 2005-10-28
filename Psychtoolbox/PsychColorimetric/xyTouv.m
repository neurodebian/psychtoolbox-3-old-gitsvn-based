function uv = xyTouv(xy)
% uv = xyTouv(xy)
%
% Convert CIE xy chromaticity to CIE u'v' chromaticity.
%
% 7/15/03  dhb, bx  Wrote it.
% 3/17/04  dhb			Fixed typos.  This must not have been tested previously.

xyY = [xy ; ones(1,size(xy,2))];
XYZ = xyYToXYZ(xyY);
uvY = XYZTouvY(XYZ);
uv = uvY(1:2,:);

% One could check with direct computation from
% published formulae (CIE, Colorimetry, p. 54.)
% We checked for a few values and then commented this out.
% uvCheck = zeros(size(uv));
% uvCheck(1) = 4*xy(1)/(-2*xy(1)+12*xy(2)+3);
% uvCheck(2) = 9*xy(2)/(-2*xy(1)+12*xy(2)+3);
