function err = Snd(command,signal,rate,sampleSize)
% err = Snd(command,[signal],[rate],[sampleSize])
% 
% Old Sound driver for Psychtoolbox. USE OF THIS DRIVER IS DEPRECATED FOR
% ALL BUT THE MOST TRIVIAL PURPOSES!
%
% Have a look at the help for PsychPortAudio ("help PsychPortAudio" and
% "help InitializePsychSound") for an introduction into the new sound
% driver, which is recommended for most purposes.
%
% While Snd used to use a special purpose low level driver on MacOS-9 which
% was well suited for cognitive science, Snd for all other operating
% systems (Windows, MacOS-X, Linux) just calls into Matlab's Sound()
% function which is of varying - but usually pretty poor - quality in most
% implementations of Matlab. There are many bugs, latency- and timing
% problems associated with the use of Snd.
%
% GNU/OCTAVE: You must install the optional "audio" package from
% Octave-Forge, as of Octave 3.0.5, otherwise the required sound() function
% won't be available and this function will fail!
%
%
% Supported functions:
% --------------------
%
% Snd('Play',signal,[rate]) plays a sound.
% OS9: Unlike Matlab's SOUND command, Snd returns immediately, while the 
% sound is still playing. The queue can have up to 128 sounds, so
% calling Snd('Play',signal) while a sound is already playing adds the new
% sound to the queue. 
%
% WIN & OSX: Snd calls the built-in MATLAB SOUND. Also see AUDIOPLAYER 
% and PLAY. High latency, unreliable timing, prone to crashing, doesn't
% return until sound is fully played, no support for queuing.
% 
% rate=Snd('DefaultRate') returns the default sampling rate in Hz, which
% currently is 22254.5454545454 Hz on all platforms. This default may
% change in the future, so please either specify a rate, or use this
% function to get the default rate. (This default is suboptimal on any
% system except MacOS-9!)
%
% Before playing, Snd internally converts the matrix of double-precision 
% floating-point values passed in the signal argument to either an array 
% of 8-bit or 16-bit values. The optional sampleSize argument used with 
% Snd('Play') specifies which.  Allowable values are either 8 or 16.  If 
% no value is specified then Snd defaults to 16. 
%
% Snd('Open') opens the channel, which stays open until you call
% Snd('Close'). Snd('Play',...) automatically opens the channel if it
% isn't already open. (At present the WIN & OSX version ignores the 'Open'
% command.)
% 
% Snd('Close') immediately stops all sound and closes the channel. (This
% happens automatically whenever Snd.mex is flushed, e.g. by "CLEAR Snd"
% or "CLEAR MEX".)
% 
% Snd('Wait') waits until all the sounds in the channel play through.
% 
% isPlaying=Snd('IsPlaying') returns true (number of sounds in channel) if
% any sound is playing, and false (0) otherwise.
% 
% Snd('Quiet') stops the sound currently playing and flushes the queue,
% but leaves the channel open.
% 
% "signal" may either be a numeric array of samples, or a string giving
% the name of a Macintosh snd resource in the System file (or Matlab
% application or MEX file). (Use the Sound control panel to see what Alert
% Sounds are available.)
% 
% Your "signal" data should lie between -1 and 1 (smaller to play more
% softly). If the "signal" array has one row then it's played monaurally,
% through both speakers. If it has two rows then it's played in stereo.
% (Snd has no provision for changing which speaker(s), or the volume, used
% by a named snd resource, so use READSND to get the snd into an array,
% and supply the appropriately modified array to Snd.)
% 
% "rate" is the rate (in Hz) at which the samples in "signal" should be
% played. We suggest you always specify the "rate" parameter. If not
% specified, the sample "rate", on all platforms, defaults to OS9's
% standard hardware sample rate of 22254.5454545454 Hz. That value is
% returned by Snd('DefaultRate'). Other values can be specified (in the
% range 1000 to 65535 Hz); linear interpolation is used to resample the
% data. Currently the "rate" is ignored when you specify the sound by
% name.
% 
% OSX & WIN: "samplesize". Snd accepts the sampleSize argument and passes 
% it to the Matlab SOUND command.  SOUND (and therefore Snd also) obeys the
% specified sampleSize value, either 8 or 16, only if it is supported by 
% your computer hardware. 
% 
% Snd('Play',sin(0:10000)); % play 22 KHz/(2*pi)=3.5 kHz tone
% Snd('Play',[sin(1:20000) zeros(1,10000);zeros(1,10000) sin(1:20000)]); % stereo
% Snd('Play','Quack');      % play named snd resource
% Snd('Wait');         		% wait until end of all sounds currently in channel
% Snd('Quiet');        		% stop the sound and flush the queue
% 
% At present, the OS9 channel's queue can hold up to 128 sounds (which
% could easily be increased--let us know). If the queue is full 
% when you call Snd('Play',signal),
% Snd waits until there is room (i.e. the current sound ends) and then
% adds its sound to the queue and returns. You can break out of that wait
% by hitting Command-Period.
% 
% Snd is similar to the Matlab SOUND command (and PlaySound.mex created by
% Malcolm Slaney of Apple Computer) with two enhancements: immediate
% (asynchronous) return and playing of named snd resources. SOUND and
% SoundPlay operate synchronously, i.e. they return only after the sound
% ends. Snd uses the VideoToolbox SndPlay1.c routine to play sounds
% asynchronously, i.e. return immediately, while the sound is still
% playing. Asynchronous operation is important in many psychophysical
% applications, because it allows you to play a sound during a dynamic
% visual stimulus.
% 
% For most of the commands, the returned value is zero when successful, 
% and a nonzero Apple error number when Snd fails. The Apple error 
% number can be looked up, e.g. in Easy Errors, available from:
% <http://hyperarchive.lcs.mit.edu/cgi-bin/NewSearch?key=Easy+Errors>
% 
% OS9: Snd('Play',signal) takes some time to open the channel, if it isn't
% already open, and allocate a snd structure for your sound. This overhead
% of the call to Snd, if you call it in the middle of a movie, may be
% perceptible as a pause in the movie, which would be bad. 
% (There is no opening overhead in the WIN & OSX version.) However, the
% actual playing of the sound, asynchronously, is a background process
% that usually has very little overhead. So, even if you want a sound to
% begin after the movie starts, you should create a soundtrack for your
% entire movie duration (possibly including long silences), and call Snd
% to set the sound going before you start your movie. (Thanks to Liz Ching
% for raising the issue.)
% 
% NOTE: When you specify a snd by name, you cannot specify a volume, but
% there is an easy work-around. As explained in the following paragraph,
% use ResEdit and READSND to get the snd into an array. When you play
% a snd from an array, you can control the volume by scaling the array.
% (Thanks to Larry James for asking.)
% 
% NOTE: Snd allows you to specify only the name of the snd resource (e.g.
% 'Quack'), not the file. Matlab's READSND and WRITESND only allow you to
% specify the filename, not the name of the resource. This is confusing
% and inconvenient. One wishes for MEX files, perhaps "SndRead" and
% "SndWrite", that would accept both the filename and the snd resource
% name. However, the need isn't pressing; you can use ResEdit to copy snd
% resources (e.g. from the System file) into individual files (one file
% per snd), which you can access with Matlab's READSND and WRITESND
% commands. You can get ResEdit from Apple's web site:
% web http://asu.info.apple.com/swupdates.nsf/artnum/n10964
% (Thanks to Gary Marcus for asking.)
% 
% Snd.mex is an enhanced version of our now-obsolete SndPlay.mex, which
% was based partly on PlaySound.c by Malcom Slaney, and partly on 
% VideoToolbox CreateTrialSounds.c. Most of the real work is done by
% the VideoToolbox routine SndPlay1.c.
% 
% ******
%	
% WIN & OSX: Snd plays your sound using Matlab's SOUND function. 
% 
% NOTE: We suggest you always specify the "rate" parameter. If not
% specified, the sample rate, on all platforms, defaults to OS9's
% standard hardware sample rate of 22254.5454545454 Hz. That value is returned
% by Snd('DefaultRate').
% 
% See also AUDIOPLAYER, PLAY, MakeBeep, READSND, and WRITESND.

% 6/6/96	dgp Wrote SndPlay.
% 6/1/97	dgp Polished help text.
% 12/10/97	dhb Updated help.
% 2/4/98	dgp Wrote Snd, based on major update of VideoToolbox SndPlay1.c.
% 3/8/00    emw Added PC notes and code.
% 7/23/00   dgp Added notes about controlling volume of named snd, and updated
%               broken link to ResEdit.
% 4/13/02   dgp Warn that the two platforms have different default sampling rate, 
%               and suggest that everyone routinely specify sampling rate to
%               make their programs platform-independent.
% 4/13/02	dgp Enhanced both OS9 and WIN versions so that 'DefaultRate'
%               returns the default sampling rate in Hz.
% 4/13/02	dgp Changed WIN code, so that sampling rate is now same on both platforms.
% 4/15/02   awi fixed WIN code.  
% 5/30/02   awi Added sampleSize argument and documented. 
%               SndTest would crash Matlab but the problem mysteriously vanished while editing
%               the Snd.m and SndTest.m files.  I've been unable to reproduce the error. 
% 3/10/05   dgp Make it clear that the Snd mex is only available for OS9. 
%               Mention AUDIOPLAYER, as suggested by Pascal Mamassian.
% 5/20/08    mk Explain that Snd() is deprecated --> Point to PsychPortAudio!
% 1/12/09    mk Make sure that 'signal' is a 2-row matrix in stereo, not 2
%               column.
% 6/01/09    mk Add compatibility with Octave-3.

global endTime;
if isempty(endTime)
    endTime = 0;
end 

sSize = 16;
err=0;

if (IsWin | IsOSX) | IsOctave
    if nargin == 0
        error('Wrong number of arguments: see Snd.');
    end
	if streq(command,'Play')
		if nargin > 4
            error('Wrong number of arguments: see Snd.');
        end
		if nargin == 4
			if isempty(sampleSize)
				sampleSize = 16;
			elseif ~((sampleSize == 8) | (sampleSize == 16))
				error('sampleSize must be either 8 or 16.');
			end
		else
			sampleSize = 16;
		end
        if nargin < 3 
            rate=22254.5454545454;
        end
		if nargin == 3
			if isempty(rate)
				rate=22254.5454545454;
			end 
		end 
        if nargin < 2
            error('Wrong number of arguments: see Snd.');
        end
        if size(signal,1) > size(signal,2)
            error('signal must be a 2 rows by n column matrix for stereo sounds.');
        end
       	WaitSecs(endTime-GetSecs); % Wait until any ongoing sound is done.
        if IsOctave
            if exist('sound') %#ok<EXIST>
                sound(signal',rate);
            else
                % Unavailable: Try to load the package, assuming its
                % installed but not auto-loaded:
                try
                    pkg('load','audio');
                catch
                end

                % Retry...
                if exist('sound') %#ok<EXIST>
                    sound(signal',rate);
                else
                    warning('Required Octave command sound() is not available. Install and "pkg load audio" the "audio" package from Octave-Forge!'); %#ok<WNTAG>
                end
            end
        else
            sound(signal',rate,sampleSize);
        end
		endTime=GetSecs+length(signal)/rate;
	elseif streq(command,'Wait')
		if nargin>1
			error('Wrong number of arguments: see Snd.');
		end
		WaitSecs(endTime-GetSecs); % Wait until any ongoing sound is done.
		err=0;
	elseif streq(command,'IsPlaying')
		if nargin>1
			error('Wrong number of arguments: see Snd.');
		end
		if endTime>GetSecs
			err=1;
		else
			err=0;
		end
	elseif streq(command,'Quiet') | streq(command,'Close')
		if nargin>1
			error('Wrong number of arguments: see Snd.');
        end
        if ~IsOctave
            clear playsnd; % Stop any ongoing sound.
        end
		endTime=0;
		err=0;
	elseif streq(command,'DefaultRate')
		if nargin>1
			error('Wrong number of arguments: see Snd.');
		end
		err=22254.5454545454; % default sampling rate in Hz.
	elseif streq(command,'Open')
		endTime=0;
	else
	  error(['unknown command "' command '"']);
	end
end


