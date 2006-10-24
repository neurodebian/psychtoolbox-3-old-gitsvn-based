function cal = CalibrateFitLinMod(cal)
% cal = CalibrateFitLinMod(cal)
%
% Fit the linear model to spectral calibration data.
%
% 3/26/02  dhb  Pulled out of CalibrateMonDrvr.
% 3/27/02  dhb  Add case of nPrimaryBases == 0.

Pmon = zeros(cal.describe.S(3),cal.nDevices*cal.nPrimaryBases);
mGammaRaw = zeros(cal.describe.nMeas,cal.nDevices*cal.nPrimaryBases);
monSVs = zeros(cal.describe.nMeas,cal.nDevices);
for i = 1:cal.nDevices
    tempMon = reshape(cal.rawdata.mon(:,i),cal.describe.S(3),cal.describe.nMeas);
    monSVs(:,i) = svd(tempMon);
    if (cal.nPrimaryBases ~= 0)
        [monB,monW] = FindLinMod(tempMon,cal.nPrimaryBases);
        for j = 1:cal.nPrimaryBases
            monB(:,j) = monB(:,j)*monW(j,cal.describe.nMeas);
        end
    else
        cal.nPrimaryBases = 1;
        monB = tempMon(:,cal.describe.nMeas);
    end
    monW = FindModelWeights(tempMon,monB);
    for j = 1:cal.nPrimaryBases
        mGammaRaw(:,i+(j-1)*cal.nDevices) = (monW(j,:))';
        Pmon(:,i+(j-1)*cal.nDevices) = monB(:,j);
    end
end

cal.S_device = cal.describe.S;
cal.P_device = Pmon;
cal.T_device = WlsToT(cal.describe.S);
cal.rawdata.rawGammaTable = mGammaRaw;
cal.rawdata.monSVs = monSVs;
