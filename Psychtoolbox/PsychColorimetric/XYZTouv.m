function uv = XYZTouv(XYZ)
% uv = XYZTouv(XYZ)
%
% Compute uv from XYZ.
%
% 10/10/93    dhb   Created by converting CAP C code.

% Find size and allocate
[m,n] = size(XYZ);
uv = zeros(2,n);

% Compute u and v
denom = [1.0,15.0,3.0]*XYZ;
uv(1,:) = (4*XYZ(1,:)) ./ denom(1,:);
uv(2,:) = (9*XYZ(2,:)) ./ denom(1,:);