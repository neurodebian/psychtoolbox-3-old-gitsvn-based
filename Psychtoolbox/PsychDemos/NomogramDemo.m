% NomogramDemo
%
% Compare shapes of different photoreceptor nomograms.
%
% 7/8/03  dhb  Wrote it.
% 7/16/03 dhb  Add Stockman Sharpe nomogram.

% Clear out
clear all; close all

% Set parameters
S = [380 5 81];
lambdaMax = [440 530 560]';
nSpectra = length(lambdaMax);

% Compute all the nomograms
T_Baylor = PhotopigmentNomogram(S,lambdaMax,'Baylor');
T_Dawis = PhotopigmentNomogram(S,lambdaMax,'Dawis');
T_Govardovskii = PhotopigmentNomogram(S,lambdaMax,'Govardovskii');
T_Lamb = PhotopigmentNomogram(S,lambdaMax,'Lamb');
T_SS = PhotopigmentNomogram(S,lambdaMax,'StockmanSharpe');

% Plot all nomograms in absorbance and absorbtance
for i = 1:nSpectra
	warning off
	figure(i); clf; set(gcf,'Position',[100 400 700 300]);
	subplot(1,2,1); hold on
	set(plot(SToWls(S),T_Baylor(i,:),'g'),'LineWidth',2);
	set(plot(SToWls(S),T_Dawis(i,:),'b'),'LineWidth',2);
	set(plot(SToWls(S),T_Govardovskii(i,:),'k'),'LineWidth',2);
	set(plot(SToWls(S),T_Lamb(i,:),'r'),'LineWidth',2);
	set(plot(SToWls(S),T_SS(i,:),'y'),'LineWidth',2);
	set(title('Linear'),'FontSize',14);
	set(xlabel('Wavelength (nm)'),'FontSize',12);
	set(ylabel('Absorbance'),'FontSize',12);
	axis([300 800 0 1]);
	subplot(1,2,2); hold on
	set(plot(SToWls(S),log10(T_Baylor(i,:)),'g'),'LineWidth',2);
	set(plot(SToWls(S),log10(T_Dawis(i,:)),'b'),'LineWidth',2);
	set(plot(SToWls(S),log10(T_Govardovskii(i,:)),'k'),'LineWidth',2);
	set(plot(SToWls(S),log10(T_Lamb(i,:)),'r'),'LineWidth',2);
	set(plot(SToWls(S),log10(T_SS(i,:)),'y'),'LineWidth',2);
	set(title('Log'),'FontSize',14);
	set(xlabel('Wavelength (nm)'),'FontSize',12);
	set(ylabel('Log Absorbance'),'FontSize',12);
	axis([300 800 -4 0]);
	warning on
end

% Now see if we can reconstruct StockmanSharpe fundamentals from
% the pieces.

% Get target spectral sensitivities and put in quantal units, normalized to 1.
load T_cones_ss2
S = S_cones_ss2;
T_quantal = SplineCmf(S_cones_ss2,QuantaToEnergy(S_cones_ss2,T_cones_ss2')',S);
for i = 1:size(T_quantal,1)
	T_quantal(i,:) = T_quantal(i,:)/max(T_quantal(i,:));
end

% Check nomogram spectra versus tabulated
load T_photopigment_ss;

% Build from pieces
clear photoreceptors
photoreceptors = DefaultPhotoreceptors('LivingHumanFovea');
photoreceptors.nomogram.S = S_photopigment_ss;
photoreceptors.absorbance = T_photopigment_ss;
photoreceptors.axialDensity.value = [0.5 0.5 0.4]';
photoreceptors = FillInPhotoreceptors(photoreceptors);
T_pieces = photoreceptors.isomerizationAbsorbtance;
for i = 1:size(T_pieces,1)
	T_pieces(i,:) = T_pieces(i,:)/max(T_pieces(i,:));
end

% Check nomogram spectra versus tabulated
for i = 1:size(T_pieces,1)
	nomogramAbsorbance(i,:) = photoreceptors.absorbance(i,:)/max(photoreceptors.absorbance(i,:));
end
figure; clf; hold on
set(plot(SToWls(S),log10(T_photopigment_ss(1,:)),'r'),'LineWidth',2);
set(plot(SToWls(S),log10(nomogramAbsorbance(1,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_photopigment_ss(2,:)),'g'),'LineWidth',2);
set(plot(SToWls(S),log10(nomogramAbsorbance(2,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_photopigment_ss(3,:)),'b'),'LineWidth',2);
set(plot(SToWls(S),log10(nomogramAbsorbance(3,:)),'k'),'LineWidth',1);
set(title('Absorbance vs. nomogram'),'FontSize',14);
axis([300 800 -8 0]);

fprintf('Calculations of sensitivities from pieces done using:\n');
fprintf('\t%s estimates for photoreceptor IS diameter\n',photoreceptors.ISdiameter.source);
fprintf('\t%s estimates for photoreceptor OS length\n',photoreceptors.OSlength.source);
fprintf('\t%s estimates for receptor specific density\n',photoreceptors.specificDensity.source);
fprintf('\t%s photopigment nomogram\n',photoreceptors.nomogram.source);
fprintf('\t%s estimates for lens density\n',photoreceptors.lensDensity.source);
fprintf('\t%s estimates for macular pigment density\n',photoreceptors.macularPigmentDensity.source);
fprintf('\t%g mm for axial length of eye\n',photoreceptors.eyeLengthMM.value);
fprintf('\n');
fprintf('Photoreceptor Type             |\t       L\t       M\t     S\n');
fprintf('______________________________________________________________________________________\n');
fprintf('\n');
fprintf('Lambda max                     |\t%8.1f\t%8.1f\t%8.1f\t nm\n',photoreceptors.nomogram.lambdaMax);
fprintf('Outer Segment Length           |\t%8.1f\t%8.1f\t%8.1f\t um\n',photoreceptors.OSlength.value);
fprintf('Inner Segment Diameter         |\t%8.1f\t%8.1f\t%8.1f\t um\n',photoreceptors.ISdiameter.value);
fprintf('\n');
fprintf('Axial Specific Density         |\t%8.3f\t%8.3f\t%8.3f\t /um\n',photoreceptors.specificDensity.value);
fprintf('Axial Optical Density          |\t%8.3f\t%8.3f\t%8.3f\n',photoreceptors.axialDensity.value);
fprintf('Peak isomerization prob.       |\t%8.3f\t%8.3f\t%8.3f\n',max(photoreceptors.isomerizationAbsorbtance,[],2));
fprintf('______________________________________________________________________________________\n');

% Plot the two
figure; clf; hold on
set(plot(SToWls(S),T_quantal(1,:),'r'),'LineWidth',2);
set(plot(SToWls(S),T_pieces(1,:),'k'),'LineWidth',1);
set(plot(SToWls(S),T_quantal(2,:),'g'),'LineWidth',2);
set(plot(SToWls(S),T_pieces(2,:),'k'),'LineWidth',1);
set(plot(SToWls(S),T_quantal(3,:),'b'),'LineWidth',2);
set(plot(SToWls(S),T_pieces(3,:),'k'),'LineWidth',1);
set(title('Linear'),'FontSize',14);
set(xlabel('Wavelength (nm)'),'FontSize',12);
set(ylabel('Absorbance'),'FontSize',12);
figure; clf; hold on
warning off
set(plot(SToWls(S),log10(T_quantal(1,:)),'r'),'LineWidth',2);
set(plot(SToWls(S),log10(T_pieces(1,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_quantal(2,:)),'g'),'LineWidth',2);
set(plot(SToWls(S),log10(T_pieces(2,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_quantal(3,:)),'b'),'LineWidth',2);
set(plot(SToWls(S),log10(T_pieces(3,:)),'k'),'LineWidth',1);
axis([300 800 -4 0]);
set(title('Log'),'FontSize',14);
set(xlabel('Wavelength (nm)'),'FontSize',12);
set(ylabel('Absorbance'),'FontSize',12);
warning on

return

% The following is not yet working.

% Now some fitting
staticParams.T_quantal = T_quantal;
staticParams.whichNomogram = 'Govardovskii';
staticParams.S = S;
staticParams.lensTransmittance = photoreceptors.lensDensity.transmittance;
staticParams.macularTransmittance = photoreceptors.macularPigmentDensity.transmittance;
staticParams.LserWeight = 0.3;
[T_fit,fitParams] = FitCones(staticParams);

% Plot the two
figure; clf; hold on
set(plot(SToWls(S),T_quantal(1,:),'r'),'LineWidth',2);
set(plot(SToWls(S),T_fit(1,:),'k'),'LineWidth',1);
set(plot(SToWls(S),T_quantal(2,:),'g'),'LineWidth',2);
set(plot(SToWls(S),T_fit(2,:),'k'),'LineWidth',1);
set(plot(SToWls(S),T_quantal(3,:),'b'),'LineWidth',2);
set(plot(SToWls(S),T_fit(3,:),'k'),'LineWidth',1);
set(title('Linear'),'FontSize',14);
set(xlabel('Wavelength (nm)'),'FontSize',12);
set(ylabel('Absorbance'),'FontSize',12);
figure; clf; hold on
warning off
set(plot(SToWls(S),log10(T_quantal(1,:)),'r'),'LineWidth',2);
set(plot(SToWls(S),log10(T_fit(1,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_quantal(2,:)),'g'),'LineWidth',2);
set(plot(SToWls(S),log10(T_fit(2,:)),'k'),'LineWidth',1);
set(plot(SToWls(S),log10(T_quantal(3,:)),'b'),'LineWidth',2);
set(plot(SToWls(S),log10(T_fit(3,:)),'k'),'LineWidth',1);
axis([300 800 -4 0]);
set(title('Log'),'FontSize',14);
set(xlabel('Wavelength (nm)'),'FontSize',12);
set(ylabel('Absorbance'),'FontSize',12);
warning on
