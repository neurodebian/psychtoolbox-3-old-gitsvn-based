function retval = SetAnaglyphStereoParameters(cmd, win, rgb)
% retval = SetAnaglyphStereoParameters(subcommand, windowPtr, [red green blue]);
%
% Change parameters of built-in anaglyph stereo display function at
% runtime. Allows to change all relevant settings of the algorithm at any
% time in your script. You need to open your onscreen window with the
% 'stereomode' parameter set to one of the anaglyph modes (i.e. one of
% 6=Red-Green anaglyph, 7=Green-Red, 8=Red-Blue, 9=Blue-Red, where
% Red-Green means "Left eye displayed in red channel, right eye displayed
% in green channel"). You also need to enable the Psychtoolbox image
% processing pipeline by setting the 'imagingmode' parameter to at least
% 'kPsychNeedFastBackingStore'.
%
% Example:
% imagingmode = kPsychNeedFastBackingStore;
% stereomode = 6;
% windowPtr = Screen('OpenWindow', screenid, bgcolor, [], [], [], stereomode, 0, imagingmode);
%
% See help PsychGLImageProcessing for an overview of the imaging pipeline.
%
% Parameters and their meaning:
%
% 'windowPtr' is the handle to a anaglyph stereo onscreen window.
%
% 'subcommand' can be one of:
%
% 'LeftGains' - Provide (in optional vector [red green blue]) per-channel
% color gains for computation of the left-eye channel. This allows to
% adjust the output colors to optimally match the color emission / filter
% profile of your anaglyph glasses and monitor - Reduce ghosting / visual
% crosstalk. E.g.
%
% SetAnaglyphStereoParameters('LeftGains', win, [1.0 0.0 0.0]); sets the
% red-gain for the left eye image to 1.0 and disables output of the left
% eye image into the green or blue channels. This is the default setting
% for Red-Green or Red-Blue anaglyph, left eye view only goes into red
% channel.
%
% The command always returns the old gains as a 3 component vector.
%
% 'RightGains' - See left gains, but this time for right image:
% SetAnaglyphStereoParameters('RightGains', win, [0.0 0.7 0.0]);
% would set the green channel to 70% output intensity and disable the red-
% and blue channels. This is a typical setting for Red-Green anaglyph
% stereo: Right eye only written to green color channel, but with reduced
% output intensity to compensate for the stronger green-sensitivity of the
% human eye.
%
% 'ColorToLuminanceWeights' - Set the color to luminance conversion
% weights: PTB converts all color images into pure luminance (greyscale)
% images before distributing them into the color channels via the formula:
% Luminance = redvalue * redweight + bluevalue * blueweight + greenvalue *
% greenweight.
%
% The default red- green- and blue-weights are (0.3, 0.59, 0.11), so:
% L = 0.3*red + 0.59*green + 0.11*blue. This is according to NTSC standard
% (if i remember correctly!). You can change the weights via:
%
% SetAnaglyphStereoParameters('ColorToLuminanceWeights', win, [redweight greenweight blueweight]);
%
% The command always returns the old weights as a 3 component vector.
%
% 'BackgroundColorBias' - Set the "background color" for inverted anaglyph
% mode. TODO: Finish...
%
% 'GetHandle' - Return GLSL handle to anaglyph shader. Allows to modify the
% shader itself, e.g., replace it by your own customized shader. Only for
% OpenGL experts!
%

% History:
% 5.1.2007 Wrote it (MK).

% Only works if GL shading language is supported.
AssertGLSL;

try
    if nargin < 2
        error('Insufficient number of input arguments.');
    end
    
    % Query GLSL shader handle for anaglyph shader:
    [slot shaderid blittercfg voidptr glsl luttexid] = Screen('HookFunction', win, 'Query', 'StereoCompositingBlit', 'StereoCompositingShader');
        
    % Shader found?
    if slot == -1
        error('Either the imaging pipeline is not enabled for given onscreen window, or it is not switched to Anaglyph stereo mode.');
    end
    
    if glsl == 0
        error('Anaglyph shader is not operational for unknown reason. Sorry...');
    end
    
    % Bind it:
    glUseProgram(glsl);
catch
    % If anything failed. Unbind:
    glUseProgram(0);
    psychrethrow(psychlasterror);
end
    
    % Subcommand dispatch:
    if strcmpi(cmd, 'GetHandle')
        retval = glsl;
    end
    
    if strcmpi(cmd, 'BackgroundColorBias')
        uniloc = glGetUniformLocation(glsl, 'ChannelBias');
        retval = glGetUniformfv(glsl, uniloc) * WhiteIndex(win);
        if nargin>=3
            if size(rgb)~=3
                error('Provided call parameter must be a 3 component vector with (R,G,B) bias color values.');
            end
            
            % Normalize from color range of window to GL's 0-1 range:
            rgb = rgb / WhiteIndex(win);
            
            glUniform3fv(uniloc, 1, rgb);
        end
    end

    if strcmpi(cmd, 'ColorToLuminanceWeights')
        uniloc = glGetUniformLocation(glsl, 'ColorToGrayWeights');
        retval = glGetUniformfv(glsl, uniloc);
        if nargin>=3
            if size(rgb)~=3
                error('Provided call parameter must be a 3 component vector with color weights or gains.');
            end
            glUniform3fv(uniloc, 1, rgb);
        end
    end
    
    if strcmpi(cmd, 'LeftGains')
        uniloc = glGetUniformLocation(glsl, 'Gains1');
        retval = glGetUniformfv(glsl, uniloc);
        if nargin>=3
            if size(rgb)~=3
                error('Provided call parameter must be a 3 component vector with color weights or gains.');
            end
            glUniform3fv(uniloc, 1, rgb);
        end
    end
    
    if strcmpi(cmd, 'RightGains')
        uniloc = glGetUniformLocation(glsl, 'Gains2');
        retval = glGetUniformfv(glsl, uniloc);
        if nargin>=3
            if size(rgb)~=3
                error('Provided call parameter must be a 3 component vector with color weights or gains.');
            end
            glUniform3fv(uniloc, 1, rgb);
        end
    end

try
    % Unbind shader again...
    glUseProgram(0);
catch
    psychrethrow(psychlasterror);
end

% Done.
return;
