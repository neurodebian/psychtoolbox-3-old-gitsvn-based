function PerceptualVBLSyncTest(screen, stereomode, fullscreen, doublebuffer, maxduration)
% PerceptualVBLSyncTest(screen, stereomode, fullscreen, doublebuffer, maxduration)
%
% Perceptual synchronization test for synchronization of Screen('Flip') and
% Screen('WaitBlanking') to the vertical retrace.
%
% Arguments:
% 'screen' Either a single screen handle, or none (in which case the
% display with the maximum id will be used), or a vector of two handles in
% stereomode 10, e.g., [0 1] if you want to output to screens 0 and 1.
%
% 'stereomode' Which stereomode to use? Defaults to zero, ie. no stereo.
%
% 'fullscreen' Fullscreen presentation? Defaults to 1 ie. yes.
%
% 'doublebuffer' Single- or double-buffering (1). Defaults to 1.
%
% 'maxduration' Maximum runtime of test: Runs until keypress or maxduration
% seconds have elapsed (Default is 10 seconds).
%
% After starting this test, you should see a flickering greyish background
% that flickers in a homogenous way - without cracks or weird moving patterns
% in the flickering area. If you see an imhogenous flicker, this means that
% synchronization of stimulus onset to the vertical retrace doesn't work due
% to some serious bug or limitation of your graphics hardware or its driver.
% If you don't know what this means, you can test this script with parameter
% doublebuffer == 0 to artificially create a synchronization failure.
%
% On OS-X you should also see some emerging pattern of yellow horizontal lines.
% These lines should be tightly concentrated/clustered in the topmost area of
% the screen. Lots of yellow lines in the middle or bottom area or even
% randomly distributed lines indicate some bug in the driver of your graphics
% hardware. This is a common problem of all ATI graphics adapters on MacOS-X
% versions earlier than OS-X 10.4.3 when running a dual-display setup...
%
% A second reason for distributed yellow lines could be bad timing on your
% machine, e.g., due to background activity by virus scanners or the Spotlight
% indexing service on OS-X. Turn these off for conducting your studies!
%

% History:
% 01/28/06 mk Written. Replaces the built-in flickertest of Screen('OpenWindow')
%             on multi-display setups. That test wasn't well received :(

% Check for presence of OpenGL PTB.
AssertOpenGL;

if nargin < 4
   % Use double-buffered windows by default: Single-buffered ones can't sync to
   % retrace and are discouraged anyway. Setting doublebuffer=0 is an easy way
   % to reproduce the visual pattern created by a complete sync-failure though.
   doublebuffer=1;
end;
doublebuffer=doublebuffer+1;

if nargin < 2
   % Use non-stereo display by default. 
   stereomode=0;
end;

if nargin < 3
   fullscreen=1;
end;

if nargin < 1
    if stereomode == 10
        screen(1) = max(Screen('Screens')) - 1;
        screen(2) = max(Screen('Screens'));
        if screen(1)<0
            error('Stereomode 10 only works on setups with two attached displays!');
        end
    else
        screen=max(Screen('Screens'));
    end
end;

if nargin < 5
    maxduration = 10;
end

try
   if fullscreen
      rect=[];
   else
      rect=[0,0,800,600];
   end;
   
   help PerceptualVBLSyncTest;
   fprintf('Press ENTER key to start the test. The test will stop after 10 seconds\n');
   fprintf('or any keypress...\n');
   pause;
   while KbCheck; end;
   
   if stereomode~=10
       % Standard case:
       [win , winRect]=Screen('OpenWindow', screen, 0, rect, 32, doublebuffer, stereomode);
   else
       % Special case for dual-window stereo:

       % Setup master window:
       [win , winRect]=Screen('OpenWindow', screen(1), 0, rect, 32, doublebuffer, stereomode);
       % Setup slave window:
       Screen('OpenWindow', screen(2), 0, rect, 32, doublebuffer, stereomode);       
   end
   
   flickerRect = InsetRect(winRect, 100, 0);
   color = 0;
   deadline = GetSecs + maxduration;
   beampos=0;
   
   VBLTimestamp = Screen('Flip', win, 0, 2);
   
   while (~KbCheck) & (GetSecs < deadline)
      % Draw left eye view (if stereo enabled):
      Screen('SelectStereoDrawBuffer', win, 0);
      % Draw alternating black/white rectangle:
      Screen('FillRect', win, color, flickerRect);
      % If beamposition is available (on OS-X), visualize it via yellow horizontal line:
      if (beampos>=0) Screen('DrawLine', win, [255 255 0], 0, beampos, winRect(3), beampos); end;
      % Same for right-eye view...
      Screen('SelectStereoDrawBuffer', win, 1);
      Screen('FillRect', win, color, flickerRect);
      if (beampos>=0) Screen('DrawLine', win, [255 255 0], 0, beampos, winRect(3), beampos); end;
      
      % Alternate drawing color from white -> black, or black -> white
      color=255 - color;
      
      if doublebuffer>1
         % Flip buffer on next vertical retrace, query rasterbeam position on flip, if available:
         [VBLTimestamp, StimulusOnsetTime, FlipTimestamp, Missed, beampos] = Screen('Flip', win, VBLTimestamp + 0.002, 2);
      	%beampos=400 + 100 * sin(GetSecs);   
      else
         % Just wait a bit in non-buffered case:
         pause(0.001);
      end;
   end;
   
   Screen('CloseAll');
   return;   
catch
   Screen('CloseAll');   
end;
