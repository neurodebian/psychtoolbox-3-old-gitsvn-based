function [nx, ny, textbounds] = DrawFormattedText(win, tstring, sx, sy, color, wrapat)
% [nx, ny, textbounds] = DrawFormattedText(win, tstring [, sx][, sy][, color][, wrapat])
%
% Draws a string of text 'tstring' into Psychtoolbox window 'win'. Allows
% some basic formatting. The text string 'tstring' may contain newline
% characters '\n'. Whenever a newline character '\n' is encountered, a
% linefeed and carriage return is performed, breaking the text string into
% lines. 'sx' defines the left border of the text: If it is
% left out, text starts at x-position zero, otherwise it starts at the
% specified position 'sx'. If sx=='center', then each line of text is
% horizontally centered in the window. 'sy' defines the top border of the
% text. If left out, it starts at the top of the window, otherwise it
% starts at the specified vertical pixel position. If sy=='center', then
% the whole text is vertically centered in the window. 'color' is the color
% value of the text (color index or [r g b] triplet or [r g b a]
% quadruple). If color is left out, the current text color from previous
% text drawing commands is used. 'wrapat', if provided, will automatically
% break text strings longer than 'wrapat' characters into newline separated
% strings of roughly 'wrapat' characters. This is done by calling the
% WrapString function (See 'help WrapString').
%
% The function returns the new (nx, ny) position of the text drawing cursor
% and the bounding rectangle 'textbounds' of the drawn string. (nx,ny) can
% be used as new start position for connecting further text strings to the
% bottom of the drawn text string. Calculation of these bounds is
% approximative, so it may give wrong results with some text fonts and
% styles on some operating systems.
%
% See DrawFormattedTextDemo for a usage example.

% History:
% 10/17/06  Written (MK).

if nargin < 1 || isempty(win)
    error('DrawFormattedText: Windowhandle missing!');
end

if nargin < 2 || isempty(tstring)
    error('DrawFormattedText: Text string missing!');
end

% Default x start position is left border of window:
if nargin < 3 || isempty(sx)
    sx=0;
end

if ischar(sx) && strcmp(lower(sx), 'center')
    xcenter=1;
    sx=0;
else
    xcenter=0;
end

% No text wrapping by default:
if nargin < 6 || isempty(wrapat)
    wrapat = 0;
end

% Text wrapping requested?
if wrapat > 0
    % Call WrapString to create a broken up version of the input string
    % that is wrapped around column 'wrapat'
    tstring = WrapString(tstring, wrapat);
end

% Convert all conventional linefeeds into C-style newlines:
newlinepos = strfind(tstring, char(10));
while ~isempty(newlinepos)
    % Replace first occurence of ASCII code 10 by a '\n':
    tstring = [ tstring(1:min(newlinepos)-1) '\n' tstring(min(newlinepos)+1:end)];
    % Search next occurence of linefeed (if any) in new expanded string:
    newlinepos = strfind(tstring, char(10));
end

% Query textsize for implementation of linefeeds:
theight = Screen('TextSize', win);

% Default y start position is top of window:
if nargin < 4 || isempty(sy)
    sy=0;
end

if ischar(sy) && strcmp(lower(sy), 'center')
    % Compute vertical centering:
    
    % Compute height of text box:
    numlines = length(strfind(tstring, '\n')) + 1;
    bbox = SetRect(0,0,1,numlines * theight);
    % Center box in window:
    [rect,dh,dv] = CenterRect(bbox, Screen('Rect', win));

    % Initialize vertical start position sy with vertical offset of
    % centered text box:
    sy = dv;
end

% Keep current text color if noone provided:
if nargin < 5 || isempty(color)
    color = [];
end

% Init cursor position:
xp = sx;
yp = sy;
nx = xp;
ny = yp;

minx = inf;
miny = inf;
maxx = 0;
maxy = 0;

% Parse string, break it into substrings at line-feeds:
while length(tstring)>0
    % Find next substring to process:
    crpositions = strfind(tstring, '\n');
    if ~isempty(crpositions)
        curstring = tstring(1:min(crpositions)-1);
        tstring = tstring(min(crpositions)+2:end);
        dolinefeed = 1;
    else
        curstring = tstring;
        tstring =[];
        dolinefeed = 0;
    end

    % tstring contains the remainder of the input string to process in next
    % iteration, curstring is the string we need to draw now.

    % Any string to draw?
    if ~isempty(curstring)
        % Horizontally centered output required?
        if xcenter
            % Yes. Compute text bounding box for this substring and compute
            % dh, dv position offsets to center it in the center of window.
            bbox=Screen('TextBounds', win, curstring);
            [rect,dh,dv] = CenterRect(bbox, Screen('Rect', win));
            % Set drawing cursor to horizontal x offset:
            xp = dh;
        end
            
        [nx ny] = Screen('DrawText', win, curstring, xp, yp, color);
    else
        % This is an empty substring (pure linefeed). Just update cursor
        % position:
        nx = xp;
        ny = yp;
    end

    % Update bounding box:
    minx = min([minx , xp, nx]);
    maxx = max([maxx , xp, nx]);
    miny = min([miny , yp, ny]);
    maxy = max([maxy , yp, ny]);

    % Linefeed to do?
    if dolinefeed
        % Update text drawing cursor to perform carriage return:
        if xcenter==0
            xp = sx;
        end
        yp = ny + theight;
    else
        % Keep drawing cursor where it is supposed to be:
        xp = nx;
        yp = ny;
    end
    % Done with substring, parse next substring.
end

% Add one line height:
maxy = maxy + theight;

% Create final bounding box:
textbounds = SetRect(minx, miny, maxx, maxy);
% Create new cursor position:
nx = minx;
ny = maxy;

return;
