function T_absorbance = StockmanSharpeNomogram(S,lambdaMax)
% T_absorbance = StockmanSharpeNomogram(S,lambdaMax)
%
% Compute normalized absorbance according to the
% nomogram provided by Stockman and Sharpe:
% http://cvrl.ioo.ucl.ac.uk/database/text/pigments/sstemplate.htm.
%
% From email from Stockman, expanding on the original web page/paper
% description:
%
%   The common or average template for the three photopigment optical density
%   spectra () for the log wavelength scale is:
% 
%    POLYNOMIAL HERE
% 
%   where x is log10(l), and l is the wavelength in nm,
% 
%    PARAMETERS HERE
%
%   The template was derived iteratively by aligning the S-cone and M-cone photopigment
%   with the L-cone photopigment spectra and then finding the best-fitting polynomial
%   to describe the aligned spectra. The mean template has a lmax of 558.0 nm.
% 
%   For other lmax values the template should be shifted along a log wavelength scale. In general, then:
%   x=log10(l) - log10(lmax/558).
% 
%   The lmax values of the fitted templates that best fits the original Stockman and Sharpe
%   (2000) S-, M- and L-cone photopigment spectra are 420.7, 530.3 and 558.9 nm for the
%   S-, M- and L-cones, respectively;
% 
%   For further details, please see Equation 8 and the discussion in Stockman and Sharpe (2000).
%
% The result is in quantal units, in the sense that to compute
% absorbtions you want to incident spectra in quanta.
% To get sensitivity in energy units, apply EnergyToQuanta().
%
% Argument lambdaMax may be a column vector of wavelengths.
%
% As near as I can tell, this funciton implements the equations,
% but I get crazy answers.  Email sent to Andrew Stockman 7/17/03.
%
% 5/8/99		dhb  Started writing it.
% 10/27/99	dhb  Added error return to prevent premature use of this routine.
% 7/18/03   dhb  Finished it off.

% Parameters
a = -188862.970810906644;
b = 90228.966712600282;
c = -2483.531554344362;
d = -6675.007923501414;
e = 1813.525992411163;
f = -215.177888526334;
g = 12.487558618387;
h = -0.289541500599;

% Set up and apply formula
wls = MakeItWls(S)';
nWls = length(wls);
nT = length(lambdaMax);
T_absorbance = zeros(nT,nWls);
for i = 1:nT
	% Get lambda max
	theMax = lambdaMax(i);
	
	% Need to normalize wavelengths
	logWlsNorm = log10(wls)-log10(theMax/558);
	
	% Compute log optical density
	logDensity = a + ...
						   b*logWlsNorm.^2 + ...
							 c*logWlsNorm.^4 + ...
							 d*logWlsNorm.^6 + ...
							 e*logWlsNorm.^8 + ...
							 f*logWlsNorm.^10 + ...
							 g*logWlsNorm.^12 + ...
							 h*logWlsNorm.^14;
	logDensity = logDensity;
	T_absorbance(i,:) = 10.^logDensity;
end

