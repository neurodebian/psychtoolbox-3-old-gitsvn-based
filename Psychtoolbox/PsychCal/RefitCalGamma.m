% RefitCalGamma
%
% Refit the gamma data from a calibration.
%
% 3/27/02  dhb  Wrote it.
% 8/26/03  dhb, jl  Allow changing dacsize of calibraiton.  Useful for dropping from 10 to 8.
% 2/2/05   dhb, dam Ask for filename to save to, rather than automatically overwrite.
% 9/26/08  dhb, ijk, tyl  Simplify naming possibilities.  Add some better help.
% 9/27/08  dhb      Fix up dacsize fitting.
%                   Clearer prompts (show default values).
% 11/19/09 dhb      Added crtSumPow option.  This works great for our FrontRoom monitor, which
%                   was not well fit by the traditional gamma model.  The work is done in
%                   function CalibrateFitGamma.  See comments there.
% 11/20/09 dhb      More terms in crtSumPow.
% 3/07/10  dhb      Call CalibrateFitLinMod as well.
% 3/08/10  dhb      Update list of fit type options.
% 5/28/10  dhb      Add yoked fitting routine to calls.  Should have no effect when yoked isn't set, but do the right thing when it is.
% 6/5/10   dhb      Update type list provided to user.
%          dhb      Better plots, using plot subroutines.

% Enter load code
defaultFileName = 'monitor';
thePrompt = sprintf('Enter calibration filename [%s]: ',defaultFileName);
newFileName = input(thePrompt,'s');
if (isempty(newFileName))
    newFileName = defaultFileName;
end
fprintf(1,'\nLoading from %s.mat\n',newFileName);
cal = LoadCalFile(newFileName);
fprintf('Calibration file %s read\n\n',newFileName);

% Print out some information from the calibration.
DescribeMonCal(cal);

% Provide information about gamma measurements
% This is probably not method-independent.
fprintf('Gamma measurements were made at %g levels\n',...
	size(cal.rawdata.rawGammaInput,1));
fprintf('Gamma table available at %g levels\n',...
	size(cal.gammaInput,1));

% Get new fit type
fprintf('Old gamma fit type was: %s\n',cal.describe.gamma.fitType);
oldType = cal.describe.gamma.fitType;
fprintf('Possible fit types are defined by routine CalibrateFitGamma\n');
fprintf('See "help CalibrateFitGamma for most up to date options\n');
fprintf('Current (June, 2010) options are:\n');
fprintf('\tsimplePower\n');
fprintf('\tcrtLinear\n');
fprintf('\tcrtPolyLinear\n');
fprintf('\tcrtGamma\n');
fprintf('\tcrtSumPow\n');
fprintf('\tbetacdf\n');
fprintf('\tsigmoid\n');
fprintf('\tweibull\n');

gamma.fitType= GetWithDefault('Enter new fit type',oldType);
cal.describe.gamma = gamma;
if (strcmp(gamma.fitType,'betacdf'))
    cal.describe.gamma.useweight = GetWithDefault('Enter weighting constant (-1 -> no weighting)',0.01);
end

% Get new number of bits
fprintf('Old DAC bits was: %d\n',cal.describe.dacsize);
oldDacsize = cal.describe.dacsize;
cal.describe.dacsize = input(sprintf('Enter new dacsize: [%d]: ',oldDacsize));
if (isempty(cal.describe.dacsize))
	cal.describe.dacsize = oldDacsize;
end

% Change dacsize?  Used every once in a while.
if (cal.describe.dacsize ~= oldDacsize)
	bits = cal.describe.dacsize;
end

% Now refit
cal = CalibrateFitLinMod(cal);
cal = CalibrateFitYoked(cal);
cal = CalibrateFitGamma(cal,2^cal.describe.dacsize);

% Put up a plot of the essential data
CalibratePlotSpectra(cal,figure(1));
CalibratePlotGamma(cal,figure(2));
drawnow;

% Option to save the refit file
saveIt = input('Save new fit data (0->no, 1->yes)? [0]: ');
if (isempty(saveIt))
	saveIt = 0;
end
if (saveIt)
    % Prompt for new file name if we're saving to a name.
    defaultFileName = newFileName;
    thePrompt = sprintf('Enter calibration filename [%s]: ',defaultFileName);
    saveFileName = input(thePrompt,'s');
    if (isempty(saveFileName))
        saveFileName = defaultFileName;
    end
    fprintf(1,'\nSaving to %s.mat\n',saveFileName);
    SaveCalFile(cal,saveFileName);
end



