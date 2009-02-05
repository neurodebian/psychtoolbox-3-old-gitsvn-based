function SimpleMovieDemo(moviename)
% Most simplistic demo on how to play a movie.
%
% SimpleMovieDemo(moviename);
%
% This bare-bones demo plays a single movie whose name has to be provided -
% including the full filesystem path to the movie - exactly once, then
% exits. This is the most minimalistic way of doing it. For a more complex
% demo see PlayMoviesDemoOSX. The remaining demos show even more advanced
% concepts like proper timing etc.
%
% The demo will play our standard DualDiscs.mov movie if the 'moviename' is
% omitted.
%
% Movieplayback works on Mac OS/X, and on MS-Windows if you install
% Quicktime-7 or later, which is available as a free download from Apple.
%

% History:
% 2/5/2009  Created. (MK)

% Check if Psychtoolbox is properly installed:
AssertOpenGL;

% Does not work on Linux yet:
if IsLinux
    error('Sorry, movie playback not yet supported on Linux.');
end

if nargin < 1
    % No moviename given: Use our default movie:
    moviename = [ PsychtoolboxRoot 'PsychDemos/QuicktimeDemos/DualDiscs.mov' ];
end

% Wait until user releases keys on keyboard:
KbReleaseWait;

% Select screen for display of movie:
screenid = max(Screen('Screens'));

% Open fullscreen window on screen, with black [0] background color:
win = Screen('OpenWindow', screenid, 0);

% Open movie file:
movie = Screen('OpenMovie', win, moviename);

% Start playback engine:
Screen('PlayMovie', movie, 1);

% Playback loop: Runs until end of movie or keypress:
while ~KbCheck
    % Wait for next movie frame, retrieve texture handle to it
    tex = Screen('GetMovieImage', win, movie);

    % Valid texture returned? A negative value means end of movie reached:
    if tex<=0
        % We're done, break out of loop:
        break;
    end;

    % Draw the new texture immediately to screen:
    Screen('DrawTexture', win, tex);

    % Update display:
    Screen('Flip', win);

    % Release texture:
    Screen('Close', tex);
end;

% Stop playback:
Screen('PlayMovie', movie, 0);

% Close movie:
Screen('CloseMovie', movie);

% Close Screen, we're done:
Screen('CloseAll');

return;
