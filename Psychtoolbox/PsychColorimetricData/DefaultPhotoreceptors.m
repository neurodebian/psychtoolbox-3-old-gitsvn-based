function photoreceptors = DefaultPhotoreceptors(kind)
% photoreceptors = DefaultPhotoreceptors(kind)
% 
% Return a structure containing default sources 
% for photoreceptor complements of various kinds.
%
% Available kinds
%   LivingHumanFovea (Default) - Human foveal cones in the eye
%
% See also:  FillInPhotoreceptors, RetIrradianceToIsoRecSec
%  IsomerizationsInEyeDemo, IsomerizationsInDishDemo 
%
% 7/25/03  dhb  Wrote it.
% 12/04/07 dhb  Added dog parameters

% Default
if (nargin < 1 | isempty(kind))
	kind = 'LivingHumanFovea';
end

% Fill it in
switch (kind)
	case 'LivingHumanFovea'
		photoreceptors.species = 'Human';
		photoreceptors.OSlength.source = 'Rodieck';
		photoreceptors.ISdiameter.source = 'Rodieck';
		photoreceptors.specificDensity.source = 'Rodieck';
		photoreceptors.lensDensity.source = 'StockmanSharpe';
		photoreceptors.macularPigmentDensity.source = 'Bone';
		photoreceptors.pupilDiameter.source = 'PokornySmith';
		photoreceptors.eyeLengthMM.source = 'Rodieck';
		photoreceptors.nomogram.source = 'StockmanSharpe';
		photoreceptors.nomogram.S = [380 1 401];
		photoreceptors.nomogram.lambdaMax = [558.9 530.3 420.7]';
		photoreceptors.types = {'FovealLCone' 'FovealMCone' 'FovealSCone'};
		photoreceptors.quantalEfficiency.source = 'Generic';
    case 'LivingDog'
		photoreceptors.species = 'Dog';
		photoreceptors.OSlength.source = 'PennDog';
		photoreceptors.ISdiameter.source = 'PennDog';
		photoreceptors.specificDensity.source = 'Generic';
		photoreceptors.lensDensity.source = 'None';
		photoreceptors.macularPigmentDensity.source = 'None';
		photoreceptors.pupilDiameter.source = 'PennDog';
		photoreceptors.eyeLengthMM.source = 'PennDog';
		photoreceptors.nomogram.source = 'Govardovskii';
		photoreceptors.nomogram.S = [380 1 401];
		photoreceptors.nomogram.lambdaMax = [555 429 506]';
		photoreceptors.types = {'LCone' 'SCone' 'Rod'};
		photoreceptors.quantalEfficiency.source = 'Generic';
	case 'GuineaPig'
		photoreceptors.species = 'GuineaPig';
		photoreceptors.OSlength.source = 'SterlingLab';
		photoreceptors.OSdiameter.source = 'SterlingLab';
		photoreceptors.ISdiameter.source = 'SterlingLab';
		photoreceptors.specificDensity.source = 'Bowmaker';
		photoreceptors.lensDensity.source = 'None';
		photoreceptors.macularPigmentDensity.source = 'None';
		photoreceptors.pupilDiameter.source = 'None';
		photoreceptors.eyeLengthMM.source = 'None';
		photoreceptors.nomogram.source = 'Govardovskii';
		photoreceptors.nomogram.lambdaMax = [529 430 500]';
		photoreceptors.nomogram.S = [380 1 401];
		photoreceptors.types = {'MCone' 'SCone' 'Rod'};
		photoreceptors.quantalEfficiency.source = 'Generic';
	otherwise
		error('Unknown photoreceptor kind specified');
end
