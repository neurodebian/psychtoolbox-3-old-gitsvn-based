function sel = RandSel(in,n)
% sel = RandSel(in,n)
%   randomly selects N elements from set IN, elements are replenished.
%   IN can have any number of dimensions and elements

% DN 2008 wrote it
% 2008-08-06 Updated it to support n selections from set

selind  = ceil(RandLim([1,n],0,numel(in)));
sel     = in(selind);