function secs = KbWait(deviceNumber)
% secs = KbWait([deviceNumber])
% 
% Waits until any key is down and returns the time in seconds. 
%
% If you have trouble with KbWait always returning immediately, this could
% be due to "stuck keys". See "help DisableKeysForKbCheck" on how to work
% around this problem.
%
% GetChar and CharAvail are character oriented (and slow), whereas KbCheck
% and KbWait are keypress oriented (and fast).
%
% Using KbWait from the MATLAB command line: When you type "KbWait" at the
% prompt and hit the enter/return key to execute that command, then KbWait
% will detect the enter/return key press and return immediatly.  If you
% want to test KbWait from the command line, then try this:
%
%  WaitSecs(0.2);KbWait
%
% OSX: ___________________________________________________________________
%
% KbWait uses the PsychHID function, a general purpose function for
% reading from the Human Interface Device (HID) class of USB devices.
%
% KbWait tests the first USB-HID keyboard device by default. Optionally
% you can pass in a 'deviceNumber' to test a different keyboard if multiple
% keyboards are connected to your machine.
% _________________________________________________________________________
%
% See also: KbCheck, GetChar, CharAvail, KbDemo.

% 3/6/97    dhb  Wrote it.
% 8/2/97    dgp  Explain difference between key and character. See KbCheck.
% 9/06/03   awi  ****** OS X-specific fork from the OS 9 version *******
%                  Added OS X conditional.   
% 7/12/04   awi  Cosmetic.  OS 9 Section. Uses IsOSX.
% 4/11/05   awi  Added to help note about testing kbWait from command line.
% 11/29/05  mk   Fixed really stupid bug: deviceNumber wasn't queried!
% 02/22/06  mk   Modified for Linux: Currently a hack.
% 10/24/06  mk   Replaced by a generic implementation that just uses KbCheck
%                in a while loop. This way we directly benefit from KbChecks
%                improvements.

if nargin==0
   while(1)
      [isDown, secs] = KbCheck;
      if isDown
         break;
      end
      % Wait for 5 msecs to prevent system overload.
      WaitSecs(0.005);      
   end
else
   if nargin==1
      while(1)
         [isDown, secs] = KbCheck(deviceNumber);
         if isDown
            break;
         end
         % Wait for 5 msecs to prevent system overload.
         WaitSecs(0.005);
      end      
   else
      if nargin > 1         
         error('Too many arguments supplied to KbWait'); 
      end
   end   
end
