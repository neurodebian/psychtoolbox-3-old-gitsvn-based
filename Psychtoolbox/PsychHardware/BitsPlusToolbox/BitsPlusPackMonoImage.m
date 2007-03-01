function theColorImage = BitsPlusPackMonoImage(theMonoImage)
% theColorImage = BitsPlusPackMonoImage(theMonoImage)
%
% In Mono++ mode, the Bits++ box uses the red and green
% channels to provide 14-bits per pixel true intensity
% resolution.  The blue channel is set to 0 to let the
% monochromatic image through.   Empirically, we concluded
% that the packing is left adjusted.  That is, the 8 MSB
% of the 14-bit input go into the red channel, and the 
% 6 LSB get left aligned in the green channel, with
% the remaining two bits set to 0.
%
% This routine packs the bits properly for this function.
%
% 11/17/03  dhb, ip	Wrote it.
% 8/13/04	dhb		Fix bug, the data were not packed quite right.
% 2/26/07   mk      Bugfix for LSB conversion: Added modulo operation.
% 3/01/07   mk      Bugfix for MSB conversion: Added floor operation.

[m,n] = size(theMonoImage);
theColorImage = uint8(zeros(m,n,3));

% Red channel gets high 8 bits
theColorImage(:,:,1) = uint8(floor(theMonoImage/256));

% Green channel gets low 6 bits, left justified
theColorImage(:,:,2) = uint8(mod(theMonoImage, 256));

