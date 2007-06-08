function InitializePsychSound(reallyneedlowlatency)
% InitializePsychSound([reallyneedlowlatency=0])
%
% This routine loads the PsychPortAudio sound driver for high-precision,
% low-latency, multi-channel sound playback.
%
% Call it at the beginning of your experiment script, optionally providing
% the 'reallyneedlowlatency' flag set to one to push really hard for low
% latency.
%
% On MacOS/X, the PsychPortAudio driver will just work with the lowest
% possible latency and highest timing precision after this initialization.
%
% On Microsoft Windows, things are more complicated and painful as always:
%
% By default PsychPortAudio on Windows will use the "portaudio_x86.dll"
% low-level sound driver included in the Psychtoolbox/PsychSound/
% subfolder. This driver supports the Windows MME (MultiMediaExtensions)
% and DirectSound sound systems. Apart from being buggy on some systems,
% these sound systems only have a mildly accurate timing and a fairly high
% inherent latency, typically over 30 milliseconds. On 150$ class sound
% hardware we were able to achieve a trial-to-trial sound onset variability
% with a standard deviation of about 1 millisecond, which is still good
% enough for many purposes.
%
% If you need really low latency or high precision sound on Windows, there's
% a second option which, for legal and technical reasons, requires a bit
% more effort and paper-work. Some (usually more expensive > 100$) sound
% cards ship with ASIO enabled sound drivers, or at least there's such a
% driver available from the support area of your sound card vendors website.
%
% Disclaimer: "ASIO is a trademark and software of Steinberg Media
% Technologies GmbH."
%
% For cards without native ASIO drivers, there's the free ASIO4ALL driver,
% downloadable from http://asio4all.com, which may or may not work well on
% your specific sound card - The driver emulates the ASIO interface on top
% of the WDM-KS (Windows Driver Model Kernel Streaming) API from Microsoft,
% so the quality depends on the underlying WDM driver. Assuming you get
% ASIO working one way or the other on your setup, you will have to get a
% special ASIO enabled version of the "portaudio_x86.dll" driver plugin for
% Psychtoolbox:
%
% 1. Contact Mario Kleiner for instructions on how to get the driver.
%
% 2. When you have the driver, copy it into your Psychtoolbox root folder -
% the top level folder named "Psychtoolbox".
%
% 3. Restart Matlab.
%
% 4. Run this function, it'll tell you if ASIO is supported.
%
% If everything suceeds and your ASIO enabled driver and sound card are of
% sufficiently high quality, you can enjoy latencies as low as 5 msecs and
% a sound onset accuracy with a standard deviation from the mean of less
% than 0.1 milliseconds - We measured around 20 microseconds on some setup.
%

% History:
% 6/6/2007 Written (MK).

if nargin < 1
    reallyneedlowlatency = [];
end

if isempty(reallyneedlowlatency)
    reallyneedlowlatency = 0; % Default: Don't push too hard for low latency.
end

% The usual tricks for MS-Windows:
if IsWin
    % Special ASIO enabled low-latency driver installed?
    if exist([PsychtoolboxRoot 'portaudio_x86.dll']) >= 2
        % Yes! Use it:
        fprintf('Detected an ASIO enhanced PortAudio driver. Good! Will try to use it.\n');
        driverloadpath = PsychtoolboxRoot;
        asio = 1;
    else
        % No :( - We use our standard driver:
        fprintf('Could not find a low-latency enhanced PortAudio driver. If you need\n');
        fprintf('really accurate sound onset timing and latency < 30 msecs, please read\n');
        fprintf('"help InitializePsychSound" carefully and follow the instructions.\n');
        fprintf('Will use our standard driver instead of enhanced driver...\n');
        driverloadpath = [PsychtoolboxRoot 'PsychSound\'];
        asio = 0;
    end

    % Standard path trick: Change working directory to driver load path,
    % preinit PsychPortAudio so it gets linked against the proper DLL, change
    % path back to old current directory:
    try
        olddir = pwd;
        cd(driverloadpath);
        % We force loading+linking+init of the driver and at the same time
        % query for ASIO support: API-Id 3 means to only query ASIO
        % devices.
        d = PsychPortAudio('GetDevices', 3);
        cd(olddir);
        
        % ASIO requested?
        if asio
            % Comply with the license...
            fprintf('\n"ASIO is a trademark and software of Steinberg Media Technologies GmbH."\n');
            
            % Found ASIO device?
            if ~isempty(d)
                % And some more commercials as required by the license...
                fprintf('Found at least one ASIO enabled soundcard in your system. Good, will try to use that!\n');
                fprintf('"ASIO Interface Technology by Steinberg Media Technologies GmbH"\n');
            else
                % ASIO PsychPortAudio driver, but no ASIO device in system!
                fprintf('PTB-Warning: Although using the ASIO enabled Psychtoolbox sound driver,\n');
                fprintf('PTB-Warning: could not find any ASIO capable soundcard in your system.\n');
                fprintf('PTB-Warning: If you think you should have an ASIO card, please check your\n');
                fprintf('PTB-Warning: system for properly installed and configured drivers and retry.\n');
                fprintf('PTB-Warning: Read "help InitializePsychSound" for more info about ASIO et al.\n');
            end
        end
    catch
        cd(olddir);
        error('Failed to load PsychPortAudio driver for unknown reason! Dependency problem?!?');
    end
end

% Maybe some tricks (in the future) for OS/X? None yet.
if IsOSX
    fprintf('PsychPortAudio initialized. Will use CoreAudio for output.\n');
end

return;
