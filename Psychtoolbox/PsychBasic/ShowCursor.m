function oldType = ShowCursor(type, screenid)
% oldType = ShowCursor([type] [, screenid])
%
% ShowCursor redisplays the mouse pointer after a previous call to
% HideCursor. If the optional 'type' is specified, it also allows to alter
% the shape of the cursor. See following sections for details.
%
% The return value 'oldType' is always zero, as this query mechanism is not
% supported with PTB-3. Just returned for backwards-compatibility.
% 
% OSX, WINDOWS, LINUX: ___________________________________________________
%
% Cursor shape can be selected. Four types are defined by name:
%
% 'Arrow' = Standard mouse-pointer arrow.
% 'CrossHair' = A cross-hair cursor.
% 'Hand' = A hand symbol.
% 'SandClock' = Some sort of sand clock/hour-glass whatever...
%
%  Apart from that names, you can pass integral numbers for type to select
%  further shapes. The mapping of numbers to shapes is operating system
%  dependent, therefore not portable across different platforms. On
%  MS-Windows, you can select between number 0 to 7. On Linux/X11 you can
%  select from a wide range of numbers from 0 up to (at least) 152, maybe
%  more, depending on your setup. See the C header file "X11/cursorfont.h"
%  for a mapping of numbers to shapes. Passing invalid numbers can create
%  errors. On OS/X, numbers between zero and 17 are currently valid. You
%  can find a list of mappings from type to number for OS/X at:
%  http://developer.apple.com/documentation/macos8/HumanInterfaceToolbox/Ap
%  pManager/ProgWithAppearanceMgr/Appearance.9d.html#10244
%
% OS9: ___________________________________________________________________ 
%
% ShowCursor.mex asks the Macintosh OS to cancel one call to HideCursor.
% You must call ShowCursor at least as many times as you've called
% HideCursor before the cursor will appear. Excess calls to ShowCursor are
% ignored. (When Screen closes its last window it calls InitCursor which
% zeroes the count.)
%
% If provided, the optional "type" argument changes the cursor shape to:
%   0: Arrow
%   1: I Beam
%   2: Cross
%   3: Plus
%   4: Watch
%   5: Arrow
% 128: P
% 300: Beachball 1/4
% 301: Beachball 2/4
% 302: Beachball 3/4
% 303: Beachball 4/4
% 400: fat arrow
% 401: fat I Beam
% Type 0 (and 5 for backward compatibility) is predefined as the standard 
% arrow cursor. The rest return whatever Apple's GetCursor(type) finds in
% the  System or Matlab's resource forks. If nothing is found, the type is
% reset to 0. The fat arrow and I beam are copied from the "Fat Cursors v
% 1.2" control panel created by Robert Abatecola, 5106 Forest Glen Drive,
% San Jose, CA 95129.
% _________________________________________________________________________

% 7/23/97  dgp Cosmetic editing.
% 8/15/97  dgp Explain hide/show counter.
% 3/15/99  xmz Added comments for PC version.
% 8/19/00  dgp Cosmetic.
% 4/14/03  awi ****** OS X-specific fork from the OS 9 version *******
%               Added call to Screen('ShowCursor'...) for OS X.
% 7/12/04  awi Divided into sections by platform.
% 11/16/04 awi Renamed Screen("ShowCursor") to Screen("ShowCursorHelper").
% 10/4/05  awi Note here that dgp made unnoted cosmetic changes between 11/16/04 and 10/4/05.
% 09/21/07 mk  Added code for selecting 'type' - the shape of a cursor - on supported systems.

% We default to setup of display screen zero, if no
% screenid provided. This argument is ignored on
% Windows and OS/X anyway. Only meaningful for
% Linux.
if nargin < 2
    screenid = 0;
end

if isempty(screenid)
    screenid = 0;
end

% Default to: No change in cursor shape...
if nargin < 1
    type = [];
else
    if ischar(type)
        % Name string provided. We can map a few symbolic names to proper
        % id's for the different operating systems:
        if strcmpi(type, 'Arrow');
            % True for Windows and OS/X:
            type = 0;
            
            if IsLinux
                type = 2;
            end
        end
        
        if strcmpi(type, 'CrossHair');
            % True for Windows:
            type = 1;
            
            if IsOSX
                type = 5;
            end
            
            if IsLinux
                type = 34;
            end
        end

        if strcmpi(type, 'Hand');
            % True for Windows:
            type = 2;
            
            if IsOSX
                type = 10;
            end

            if IsLinux
                type = 58;
            end
        end

        if strcmpi(type, 'SandClock');
            % True for Windows:
            type = 6;
            
            if IsOSX
                type = 7;
            end

            if IsLinux
                type = 26;
            end
        end

        if ischar(type)
            sca;
            error('Unknown "type" shape specification passed to ShowCursor()!');
        end
    end
end

% New cursor shape requested?
if isempty(type)
    % Only unhide / show cursor, don't modify its shape:
    % Use Screen to emulate ShowCursor.mex
    Screen('ShowCursorHelper', screenid);
else
    % Cursor shape change requested as well. Mapping of
    % types to shapes is highly OS dependent...
    Screen('ShowCursorHelper', screenid, type);
end

% Return a dummy oldtype, we don't have this info...
oldtype = 0;
