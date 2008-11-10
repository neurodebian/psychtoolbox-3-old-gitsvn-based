function VideoCaptureDemo(fullscreen, fullsize, roi)
% VideoCaptureDemo([fullscreen=0][, fullsize=0][, roi=[0 0 640 480]])

AssertOpenGL;
screen=max(Screen('Screens'));
if nargin < 1
    fullscreen=[];
end

if isempty(fullscreen)
    fullscreen=0;
end;

if nargin < 2
    fullsize=[];
end

if isempty(fullsize)
    fullsize=0;
end

if nargin < 3
    roi = [];
end

if isempty(roi)
    roi = [0 0 640 480];
end

screenid=max(Screen('Screens'));

try
    if fullscreen<1
        win=Screen('OpenWindow', screen, screenid, [0 0 800 600]);
    else
        win=Screen('OpenWindow', screen, screenid);
    end;

    % Initial flip to a blank screen:
    Screen('Flip',win);

    % Set text size for info text. 24 pixels is also good for Linux.
    Screen('TextSize', win, 24);
    
    grabber = Screen('OpenVideoCapture', win, 0, roi);
%     brightness = Screen('SetVideoCaptureParameter', grabber, 'Brightness',383)
%     exposure = Screen('SetVideoCaptureParameter', grabber, 'Exposure',130)
%     gain = Screen('SetVideoCaptureParameter', grabber, 'Gain')
%     gamma = Screen('SetVideoCaptureParameter', grabber, 'Gamma')
%     shutter = Screen('SetVideoCaptureParameter', grabber, 'Shutter',7)
%     Screen('SetVideoCaptureParameter', grabber, 'PrintParameters')
%     vendor = Screen('SetVideoCaptureParameter', grabber, 'GetVendorname')
%     model  = Screen('SetVideoCaptureParameter', grabber, 'GetModelname')

    Screen('StartVideoCapture', grabber, 30, 1);

    dstRect = [];
    oldpts = 0;
    count = 0;
    t=GetSecs;
    while (GetSecs - t) < 600 
        if KbCheck
            break;
        end;
        
        [tex pts nrdropped]=Screen('GetCapturedImage', win, grabber, 1); %#ok<NASGU>
        % fprintf('tex = %i  pts = %f nrdropped = %i\n', tex, pts, nrdropped);
        
        if (tex>0)
            % Perform first-time setup of transformations, if needed:
            if fullsize & (count == 0) %#ok<AND2>
                texrect = Screen('Rect', tex);
                winrect = Screen('Rect', win);
                sf = min([RectWidth(winrect) / RectWidth(texrect) , RectHeight(winrect) / RectHeight(texrect)]);
                dstRect = CenterRect(ScaleRect(texrect, sf, sf) , winrect);
            end

            % Setup mirror transformation for horizontal flipping:
            
            % xc, yc is the geometric center of the text.
            %[xc, yc] = RectCenter(Screen('Rect', win));
            
            % Make a backup copy of the current transformation matrix for later
            % use/restoration of default state:
            %Screen('glPushMatrix', win);
            % Translate origin into the geometric center of text:
            %Screen('glTranslate', win, xc, 0, 0);
            % Apple a scaling transform which flips the direction of x-Axis,
            % thereby mirroring the drawn text horizontally:
            %Screen('glScale', win, -1, 1, 1);
            % We need to undo the translations...
            %Screen('glTranslate', win, -xc, 0, 0);
            % The transformation is ready for mirrored drawing:

            % Draw new texture from framegrabber.
            Screen('DrawTexture', win, tex, [], dstRect);

            %Screen('glPopMatrix', win);

            % Print pts:
            Screen('DrawText', win, sprintf('%.4f', pts - t), 0, 0, 255);
             if count>0
                % Compute delta:
                delta = (pts - oldpts) * 1000;
                oldpts = pts;
                Screen('DrawText', win, sprintf('%.4f', delta), 0, 20, 255);
            end;
            
            % Show it.
            Screen('Flip', win);
            Screen('Close', tex);
            tex=0;
        end;        
        count = count + 1;
    end;
    telapsed = GetSecs - t
    Screen('StopVideoCapture', grabber);
    Screen('CloseVideoCapture', grabber);
    Screen('CloseAll');
    avgfps = count / telapsed
catch
   Screen('CloseAll');
end;
