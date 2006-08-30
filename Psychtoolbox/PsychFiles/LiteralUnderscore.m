function str = LiteralUnderscore(instr)
% str =  LiteralUnderscore(instr)
%
% Some Matlab printing and plotting routines treat an
% underscore as an instruction to subscript the next
% character.  Calling this routine inserts a "\" before
% any "_" in the passed string, so that it will come
% out as passed.

% 10/28/97  dhb  LiteralUnderscr: Wrote it.
% 2/17/97		dgp  LiteralUnderscore: new name.

n = length(instr);
outi = 1;
for i = 1:n
	if (instr(i) == '_')
		str(outi) = '\';
		str(outi+1) = '_';
		outi = outi+2;
	else
		str(outi) = instr(i);
		outi = outi+1;
	end
end
