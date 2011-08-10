close all; clear all

% stair input
probeset    = -15:0.5:15;       % set of possible probe values
meanset     = -10:0.2:10;       % sampling of pses, doesn't have to be the same as probe set
slopeset    = [.5:.1:5].^2;     % set of slopes, quad scale
guess       = 0.05;             % guess rate

% general settings
ntrial  = 40;
qpause  = false;    % pause after every iteration? (press any key to continue)
qplot   = false;    % plot information about each trial? (this pauses as well, regardless of whether you specified qpause as true)

% model observer using cum normal for psychometric function
qusemodel = true;   % use model observer to get responses or, if false, input responses by hand (0 or 1)?
truepse = 0;
truedl  = 5;
resp    = @(probe)  (guess/2 + (1-guess)*normcdf((probe-truepse)/(truedl*sqrt(0.5)/erfinv(0.5)))) > rand;


% init stair
stair = MinExpEntStair;
% option: use logistic
% stair('set_psychometric_func','logistic');
% now init
stair('init',probeset,meanset,slopeset,guess);
% option: use a subset of all data for choosing the next probe, use
% proportion of available data (good idea for robustness - see docs)
stair('toggle_use_resp_subset_prop',10,.9);

for ktrial = 1:ntrial
    % trial
    [p,entexp,ind]  = stair('get_next_probe');      % get next probe to test
    fprintf('%d, new sample point: %f\nexpect ent: %f\n', ...
        ktrial,p,entexp(ind));
    
    if qusemodel % set whether model creates response or you do by manual input
        % get observer response from model observer
        r = resp(p);
        fprintf('response: %d\n',r);
    else
        % make the response yourself, provide either 0 or 1 (actually,
        % anything below and including 0 or anything above 0)
        r = input(sprintf('r(%d): ',ktrial));
        qpause = false;
    end
    stair('process_resp',r);                        % store response in staircase
    % end trial
    
    if ktrial == ntrial || qplot
        
        [m,s,loglik]    = stair('get_fit');
        [ps,rs]         = stair('get_history');
        
        figure(1);
        subplot(1,3,1);
        imagesc(exp(loglik));
        set(gca,'YTick',1:4:length(slopeset));
        set(gca,'YTickLabel',slopeset(1:4:end));
        set(gca,'XTick',1:5:length(meanset));
        set(gca,'XTickLabel',meanset(1:5:end));
        title('estimated likelihood function');
        xlabel('PSE')
        ylabel('psychometric function slope')
        
        subplot(1,3,2);
        hold off;
        if ~isempty(entexp)
            plot(probeset,entexp,'k-o');
            hold on;
            plot(ps(ktrial)*[1,1],[min(entexp),max(entexp)],'r-');
        else
            plot(ps(ktrial)*[1,1],[0,1],'r-');
        end
        title('expected entropy vs probe');
        xlabel('possible probe values')
        xlim([min(probeset) max(probeset)]);
        
        subplot(1,3,3);
        pind = find(rs>0);
        nind = setdiff(1:length(ps),pind);
        plot(1:length(ps),ps,'k-',pind,ps(pind),'bo',nind,ps(nind),'ro');
        ylim([min(probeset) max(probeset)]);
        title('probe-resp history');
    end
    
    % pause if needed
    if (ktrial ~= ntrial) && (qpause || qplot)
        pause;
    end
    
end % loop over trials

%%% show final results
%  [mu,sigma] = stair('get_fit');    % get fitted parameters of cumulative Gaussian
%  DL = sigma*erfinv(.5)*sqrt(2)     % convert sigma (std) to DL (75% point)
% get DL from staircase directly, NB: the space of the outputted
% loglikelihood is the mean/slope space as defined atop this script, its
% not a PSE/DL space
[PSEfinal,DLfinal,loglikfinal]  = stair('get_PSE_DL');
finalent                        = sum(-exp(loglikfinal(:)).*loglikfinal(:));
fprintf('final estimates:\nm: %f\nd: %f\nent: %f\n',PSEfinal,DLfinal,finalent);
% for actual offline fitting of your data, you would probably want to use a
% dedicated toolbox such as Prins, N & Kingdom, F. A. A. (2009) Palamedes:
% Matlab routines for analyzing psychophysical data.
% http://www.palamedestoolbox.org.
% Also note that while the staircase runs far more rebust when a small
% guess rate is assumed, it is common to either fit the psychometric
% function without a guess rate, or otherwise with the guess rate as a free
% parameter (possibily varying only over subjects, but not over conditions
% within each subject).

figure(2);
imagesc(exp(loglikfinal));
set(gca,'YTick',1:4:length(slopeset));
set(gca,'YTickLabel',slopeset(1:4:end));
set(gca,'XTick',1:5:length(meanset));
set(gca,'XTickLabel',meanset(1:5:end));
xlabel('$\mu$','interpreter','latex')
switch  stair('get_psychometric_func')
    case 'cumGauss'
        title('estimated likelihood function - cumulative Gaussian')
        ylabel('$\sigma$','interpreter','latex')
    case 'logistic'
        title('estimated likelihood function - logistic')
        ylabel('$s$','interpreter','latex')
end