function [xo, yo] = RemapMouse(win, viewId, xm, ym)
%   [xo, yo] = RemapMouse(win, viewId, xm, ym); -- Map mouse position to stimulus position.
%
%   Certain operations, e.g., PsychImaging('AddTask', ..., 'GeometryCorrection', ...);
%   to perform geometric display correction, will break the 1:1
%   correspondence between stimulus pixel locations (xo,yo) and the mouse
%   cursor position, ie. a mouse cursor positioned at display position
%   (xm,ym) will be no longer pointing to stimulus pixel (xm,ym). If you
%   want to know which pixel in your original stimulus image corresponds to
%   a specific physical display pixel (or mouse cursor position), use this
%   function to perform the neccessary coordinate transformation.
%
%   'win' is the window handle of the onscreen window to remap.
%
%   'viewId' is the view id to remap, the same name you specified in
%   PsychImaging() to setup a geometry correction, e.g., 'AllViews'.
%
%   (xm,ym) is the 2D position in display device space, e.g., as returned
%   from GetMouse() for a mouse cursor position query. Please make sure
%   that you pass in the 'win'dow handle to GetMouse as well, ie.
%   GetMouse(win), so GetMouse() can correct its returned mouse position
%   for your specific display arrangement and onscreen window placement.
%   This function only corrects for distortions inside the onscreen window
%   'win', ie. relative to its origin.
%
%   The returned values (xo, yo) are the remapped stimulus pixel locations,
%   over which a mouse cursor at location (xm,ym) is currently hovering.
%
%   If you pass in a (xm,ym) position for which there doesn't exist any
%   corresponding stimulus pixel position, the values (0,0) will be
%   returned.
%
%   If you call this function on a window or view without active geometry
%   manipulations, it will do nothing and simply return the passed in (xm,
%   ym) position, ie. it is safe to use this function all the time.
%
%   Limitations: The function currently only corrects for distortions
%   introduced by the tasks implemented in the Psychtoolbox image
%   processing pipeline via some of the functions available via
%   PsychImaging() function. It does not take other transformations into
%   account, e.g., mirroring, arranging displays of a multi-display setup
%   in an unusual way etc. You may need to add your own code to take such
%   transformations into account.
%

% History:
% 26.12.2011  mk  Written.

% This global array is setup by PsychImaging() when setting up geometric
% display correction:
global ptb_geometry_inverseWarpMap;

if nargin < 4
    error('At least one of the required parameters is missing!');
end

if ~isnumeric(win) || ~isscalar(win) || (Screen('WindowKind', win) ~= 1)
    error('Window handle invalid. Does not correspond to an onscreen window!');
end

if ~ischar(viewId)
    error('viewId parameter is not a name string, as required!');
end

if isempty(ptb_geometry_inverseWarpMap) || isempty(ptb_geometry_inverseWarpMap{win})
    % Window does not use geometry correction. We're a no-op and just
    % pass-through our input values:
    xo = xm;
    yo = ym;
    return;
end

% Apply gains and modulo transform to input (xm,ym):
xm = mod(xm * ptb_geometry_inverseWarpMap{win}.gx, ptb_geometry_inverseWarpMap{win}.mx);
ym = mod(ym * ptb_geometry_inverseWarpMap{win}.gy, ptb_geometry_inverseWarpMap{win}.my);

if ~isfield(ptb_geometry_inverseWarpMap{win}, viewId)
    % No inverse map, we're done:
    xo = xm;
    yo = ym;
    return;
end

% Retrieve proper inverse mapping matrix for given viewId:
map = ptb_geometry_inverseWarpMap{win}.(viewId);

% Round and map (xm,ym) input mouse position to indices in inverse map:
xm = min(max(round(xm) + 1, 1), size(map, 2));
ym = min(max(round(ym) + 1, 1), size(map, 1));

% Lookup corresponding unmapped position from matrix:
xo = double(map(ym, xm, 1));
yo = double(map(ym, xm, 2));

return;
