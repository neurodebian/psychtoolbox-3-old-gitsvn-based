function densities = PhotopigmentAxialDensity(receptorTypes,species,source)
% densities = PhotopigmentAxialDensity(receptorTypes,[species],[source])
%
% Return estimates of photopigment axial density, sometimes called peak
% absorbance.
%
% Allowable receptor types depend on species and source, but the general
% list is:
% 	SCone, MCone, LCone, FovealSCone, FovealMCone, FovealLCone, Rod.
%
% The type argument may be a single string or a cell array of strings.  If it
% is an array, a column vector of values is returned.
%  
% The foveal version of cone types is sensible only for primates.  Not all
% estimate sources support all receptor types.
%
% Note that the following three numbers are overdetermined: photopigment
% specific density (sd), photopigment axial density (ad), and outer segment
% length osl.  In particular, ad = sd*osl.  Depending on the measurement
% method, different sources provide different pairs of these numbers.
% We have attempted to enforce this consistency in the set of routines
% PhotopigmentSpecificDensity, PhotopigmentAxialDensity, and PhotoreceptorDimensions.
% That is to say, for the same source, species, and cone type, you should get
% a consistent triplet of numbers. 
% 
% Supported species:
%		Human (Default).
%
% Supported sources:
% 	Rodieck (Human) (Default).
%   StockmanSharpe (Human).
%
% 7/11/03  dhb  Wrote it.

% Fill in defaults
if (nargin < 2 | isempty(species))
	species = 'Human';
end
if (nargin < 3 | isempty(source))
	source = 'Rodieck';
end

% Fill in specific density according to specified source
if (iscell(receptorTypes))
	densities = zeros(length(receptorTypes),1);
else
	densities = zeros(1,1);
end
for i = 1:length(densities)
	if (iscell(receptorTypes))
		type = receptorTypes{i};
	elseif (i == 1)
		type = receptorTypes;
	else
		error('Argument receptorTypes must be a string or a cell array of strings');
	end

	switch (source)
		case {'Rodieck'}
			switch (species)
				case {'Human'},
					% Rodieck, The First Steps in Seeing, Appendix B.
					switch (type)
						case {'FovealLCone','FovealMCone','FovealSCone'}
							densities(i) = 0.50;
						case 'Rod'
							densities(i) = 0.47;
						otherwise,
							error(sprintf('Unsupported receptor type %s for %s estimates in %s',type,source,species));
					end
				otherwise,
					error(sprintf('%s estimates not available for species %s',source,species));
			end	

		case {'StockmanSharpe'}
			switch (species)
				case {'Human'},
					% Foveal values from Note c, Table 2, Stockman and Sharpe (2000), Vision Research.  These
					% are the values they used to produce a fit to their 2-degree fundamentals.  The peripheral
					% values were provided to me by Andrew Stockman, and were used to produce a fit to their
					% 10-degree fundamentals.
					switch (type)
						case {'FovealLCone','FovealMCone'}
							densities(i) = 0.50;
						case 'FovealSCone'
							densities(i) = 0.40;
						case {'LCone' 'MCone'}
							densities(i) = 0.38;
						case {'SCone'}
							densities(i) = 0.3;
						otherwise,
							error(sprintf('Unsupported receptor type %s for %s estimates in %s',type,source,species));
					end
				otherwise,
					error(sprintf('%s estimates not available for species %s',source,species));
			end	

		otherwise
			error(sprintf('Unknown source %s for specific density estimates',source));
	end
end
