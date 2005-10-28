function [spd_out] = SplineSpd(wls_in, spd_in, wls_out, extend)
% [spd_out] = SplineSpd(wls_in, spd_in, wls_out, [extend])
%
% Convert the wavelength representation of a spectral
% power distribution by using a cubic spline.  Takes
% change of deltaLambda into account to keep matrix computations
% consistent across wavelength samplings.
%
% Truncates to zero outside the range of the input spectrum, unless
% extend == 1.  In this case, it extends in each direction with the
% last available value.
%
% spd_in may have multiple columns, in
% which case srf_out does as well.
%
% wls_in and wls_out may be specified as a column vector of
% wavelengths or as a [start delta n] description.
%
% 5/6/98  dhb  Change normalization method so that sum is constant.
%              This is a little closer to the desired result for
%              functions with big derivatives.
% 12/7/98 dhb  Remove 5/6/98 change, as it produces the wrong power
%              when you spline across different wavelength regions.
% 7/26/03 dhb  Add extend argument and pass to SplineRaw.

if (nargin < 4)
	extend = [];
end
spd_raw = SplineRaw(wls_in,spd_in,wls_out,extend);

% Now take change in deltaLambda into account in power measure
S_in = MakeItS(wls_in);
S_out= MakeItS(wls_out);
convertPower = S_out(2)/S_in(2);
spd_out = convertPower*spd_raw;
