function cal = CalibrateFitGamma(cal)
% cal = CalibrateFitGamma(cal)
%
% Fit the gamma function to the calibration measurements.  Options for field
% cal.describe.gamma.fitType are:
%    crtPolyLinear
%    crtGamma
%    simplePower
%
% See also PsychGamma.
%
% 3/26/02  dhb  Pulled out of CalibrateMonDrvr.
% 11/14/06 dhb  Define nInputLevels and pass to underlying fit routine.
% 07/22/07 dhb  Add simplePower fitType.

% Set nInputLevels
nInputLevels = 1024;

% Fit gamma functions.
switch(cal.describe.gamma.fitType)
    case 'crtPolyLinear',
        % For fitting, we set to zero the raw data we
        % believe to be below reliable measurement threshold (contrastThresh).
        % Currently we are fitting both with polynomial and a linear interpolation,
        % using the latter for low measurement values.  The fit break point is
        % given by fitBreakThresh.   This technique was developed
        % through bitter experience and is not theoretically driven.
        mGammaMassaged = cal.rawdata.rawGammaTable(:,1:cal.nDevices);
        massIndex = find(mGammaMassaged < cal.describe.gamma.contrastThresh);
        mGammaMassaged(massIndex) = zeros(length(massIndex),1);
        for i = 1:cal.nDevices
            mGammaMassaged(:,i) = MakeMonotonic(HalfRect(mGammaMassaged(:,i)));
        end
        fitType = 7;
        [mGammaFit1a,cal.gammaInput,mGammaCommenta] = FitDeviceGamma(...
            mGammaMassaged,cal.rawdata.rawGammaInput,fitType,nInputLevels);
        fitType = 6;
        [mGammaFit1b,cal.gammaInput,mGammaCommentb] = FitDeviceGamma(...
            mGammaMassaged,cal.rawdata.rawGammaInput,fitType,nInputLevels);
        mGammaFit1 = mGammaFit1a;
        for i = 1:cal.nDevices
            indexLin = find(mGammaMassaged(:,i) < cal.describe.gamma.fitBreakThresh);
            if (~isempty(indexLin))
                breakIndex = max(indexLin);
                breakInput = cal.rawdata.rawGammaInput(breakIndex);
                inputIndex = find(cal.gammaInput <= breakInput);
                if (~isempty(inputIndex))
                    mGammaFit1(inputIndex,i) = mGammaFit1b(inputIndex,i);
                end
            end
        end

        % Higher order components do not have this constraint and are fit with
        % a brute force homogeneous polynomial.
        if (cal.nPrimaryBases > 1)
            [m,n] = size(mGammaFit1);
            mGammaFit2 = zeros(m,cal.nDevices*(cal.nPrimaryBases-1));
            for j = 1:cal.nDevices*(cal.nPrimaryBases-1)
                mGammaFit2(:,j) = ...
                    FitGammaPolyR(cal.rawdata.rawGammaInput,cal.rawdata.rawGammaTable(:,cal.nDevices+j), ...
                    cal.gammaInput);
            end
            mGammaFit = [mGammaFit1 , mGammaFit2];
        else
            mGammaFit = mGammaFit1;
        end

    case 'crtGamma',
        mGammaMassaged = cal.rawdata.rawGammaTable(:,1:cal.nDevices);
        for i = 1:cal.nDevices
            mGammaMassaged(:,i) = MakeMonotonic(HalfRect(mGammaMassaged(:,i)));
        end
        fitType = 2;
        [mGammaFit1a,cal.gammaInput,mGammaCommenta] = FitDeviceGamma(...
            mGammaMassaged,cal.rawdata.rawGammaInput,fitType,nInputLevels);
        mGammaFit1 = mGammaFit1a;

        % Higher order components do not have this constraint and are fit with
        % a brute force homogeneous polynomial.
        if (cal.nPrimaryBases > 1)
            [m,n] = size(mGammaFit1);
            mGammaFit2 = zeros(m,cal.nDevices*(cal.nPrimaryBases-1));
            for j = 1:cal.nDevices*(cal.nPrimaryBases-1)
                mGammaFit2(:,j) = ...
                    FitGammaPolyR(cal.rawdata.rawGammaInput,cal.rawdata.rawGammaTable(:,cal.nDevices+j), ...
                    cal.gammaInput);
            end
            mGammaFit = [mGammaFit1 , mGammaFit2];
        else
            mGammaFit = mGammaFit1;
        end

    case 'simplePower',
        mGammaMassaged = cal.rawdata.rawGammaTable(:,1:cal.nDevices);
        for i = 1:cal.nDevices
            mGammaMassaged(:,i) = MakeMonotonic(HalfRect(mGammaMassaged(:,i)));
        end
        fitType = 1;
        [mGammaFit1a,cal.gammaInput,mGammaCommenta] = FitDeviceGamma(...
            mGammaMassaged,cal.rawdata.rawGammaInput,fitType,nInputLevels);
        mGammaFit1 = mGammaFit1a;

        % Higher order components do not have this constraint and are fit with
        % a brute force homogeneous polynomial.
        if (cal.nPrimaryBases > 1)
            [m,n] = size(mGammaFit1);
            mGammaFit2 = zeros(m,cal.nDevices*(cal.nPrimaryBases-1));
            for j = 1:cal.nDevices*(cal.nPrimaryBases-1)
                mGammaFit2(:,j) = ...
                    FitGammaPolyR(cal.rawdata.rawGammaInput,cal.rawdata.rawGammaTable(:,cal.nDevices+j), ...
                    cal.gammaInput);
            end
            mGammaFit = [mGammaFit1 , mGammaFit2];
        else
            mGammaFit = mGammaFit1;
        end

    otherwise
        error('Unsupported gamma fit string passed');

end

% Save information in form for calibration routines.
cal.gammaFormat = 0;
cal.gammaTable = mGammaFit;
