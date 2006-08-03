function [T_out] = SplineCmf(wls_in, T_in, wls_out, extend)
% [T_out] = SplineCmf(wls_in, T_in, wls_out, [extend])
%
% Convert the wavelength representation of a color
% matching function by using a cubic spline.
%
% Truncates to zero outside the range of the input spectrum, unless
% extend == 1.  In this case, it extends in each direction with the
% last available value.
%
% T_in may have multiple rows, in which case T_out does as well.
%
% wls_in and wls_out may be specified as a column vector of
% wavelengths or as a [start delta num] description.
% 
% 7/26/03 dhb  Add extend argument and pass to SplineRaw.
% 8/22/05 pbg  Changed T_out to include the extend variable (previously was
%              hardwired to "1".

if (nargin < 4)
	extend = [];
end
T_out = SplineRaw(wls_in,T_in',wls_out,extend)';

