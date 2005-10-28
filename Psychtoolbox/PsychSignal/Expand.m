function A=Expand(A,horizontalFactor,verticalFactor)
% A=Expand(A,horizontalFactor,[verticalFactor])
%
% Expands the ND matrix A by cell replication, and returns the result.
% If the vertical scale factor is omitted, it is assumed to be 
% the same as the horizontal. Note that the horizontal-before-vertical
% ordering of arguments is consistent with image processing, but contrary 
% to Matlab's rows-before-columns convention.
%
% We use "Tony's Trick" to replicate a vector, as explained
% in MathWorks Matlab Technote 1109, section 4.
%
% Also see ScaleRect.m

% Denis Pelli 5/27/96, 6/14/96, 7/6/96
% 7/24/02 dgp Support an arbitrary number of dimensions.

if nargin<2 | nargin>3
	error('Usage: A=Expand(A,horizontalFactor,[verticalFactor])');
end
if nargin==2
	verticalFactor=horizontalFactor;
end
if round(verticalFactor)~=verticalFactor | verticalFactor<1 ...
	round(horizontalFactor)~=horizontalFactor | horizontalFactor<1
	error('Expand only supports positive integer factors.');
end
if isempty(A)
	error('Can''t expand an empty matrix');
end
if horizontalFactor~=1
	A=shiftdim(A,1);
	siz=size(A);
	A=reshape(A,1,prod(size(A)));
	A=A(ones(1,horizontalFactor),:);
	siz(1)=siz(1)*horizontalFactor;
	A=reshape(A,siz);
	A=shiftdim(A,length(siz)-1);
end
if verticalFactor~=1
	siz=size(A);
	A=reshape(A,1,prod(size(A)));
	A=A(ones(1,verticalFactor),:);
	siz(1)=siz(1)*verticalFactor;
	A=reshape(A,siz);
end
