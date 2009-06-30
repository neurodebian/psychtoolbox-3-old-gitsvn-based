function PsychtoolboxRegistration(isUpdate, flavor)
% PsychtoolboxRegistration(isUpdate, flavor) - Online registration.
%
% This function is used to register your working copy of Psychtoolbox
% with the official www.psychtoolbox.org website. The routine is
% normally called by PsychtoolboxPostInstallRoutine at the end of
% each successfull update or initial download of a copy of Psychtoolbox.
%
% The routine is fail-safe in the sense that it will not prevent you
% from running PTB if online registration fails for some reason.
%
% The routine transmits a bit of information about your copy of PTB and
% your system environment to the website, together with a world-wide unique
% identifier of your computer+operating system. The unique identifier is
% the MAC address of your computers primary network adapter. We need this
% to disambiguate multiple downloads/updates from the same user so we
% do not count the same system setup multiple times in our statistics.
% We have no way to easily find out who you are based on this information and
% could not care less about that information.
%
% We collect this information exclusively for the following purpose:
%
% 1. Statistics about total number of downloads for the purpose of
%    documenting the use of PTB.
%
% 2. Statistics about distribution of user base wrt. operating system,
%    and Matlab / Octave version to prioritize development for the most
%    common platform+OS combinations.
%

%
% Technical Notes:
% The minimal network server for collecting the transmitted information would be the
% following line of code, executed in the Linux/OS-X command window:
%
% Linux: while true; do nc -l -p 2000 | head -c 500 >> /tmp/testlog ; done
% MacOS-X: while true; do nc -l -p 2000 >> /tmp/testlog ; done
%
% A server script that does this can be found in managementtools/psychregistrationserver.sh
%
% This will receive data from network port 2000 (any number between 1025 and 65535
% is valid), discard everything except the first 500 bytes (to prevent overflows due
% to bugs or hacker attacks) and append it to the logfile /tmp/testlog, one
% line per registered copy of Psychtoolbox.
%
% This M-File makes use of two GPL'ed free software utilities for
% M$-Windows, "nc" and "macid". Credits for the utilities and their source
% code can be found in the PsychContributed subfolder of Psychtoolbox.
%
% History:
% 18.08.2006 Written (MK)
% 5.10.2006  Add queries for Matlabs computer string and for machine
%            architecture.
% 31.3.2008  Allow spaces in path to netcat command on Windows (Fix contributed by Tobias Wolf)
% 13.2.2009  Remove need for netcat on Matlab runtimes, esp. Windows, so we
%            can get rid of our own nc.exe netcat distribution which made
%            trouble for users with misconfigured virus scanners. (MK).

% Address and port number of our statistics server:
ptbserveraddress = 'platypus.psych.upenn.edu';

% Running under OpenGL-PTB? Otherwise we abort.
AssertOpenGL;

try
    
    % All arguments supplied? Otherwise we skip registration.
    if nargin < 2
        fprintf('PsychtoolboxRegistration: WARNING - Call arguments missing. Skipped...\n');
        return;
    end;
    
    if IsOctave
        % No registration for GNU/Octave versions older than 3:
        if sscanf(version, '%i') < 3
            % Pre Octave-3: Just return -- Noop:
            fprintf('PsychtoolboxRegistration: Octave version < 3 detected. Skipping registration...\n');
            return;
        end
    end;
    
    % Default path and name for netcat command:
    nccommand = 'nc';
    
    compinfo = Screen('Computer');

    % Query OS-Type:
    if IsOSX
        ostype = 'MacOS-X';
        osversion = compinfo.system;
        % Query machine architecture via Unix 'arch' utility.
        [rc arch] = system('arch');
        arch=deblank(arch);
    end

    if IsWin
        ostype = 'Windows';
        if ~IsOctave
            try
                osversion = deblank([system_dependent('getos'),' ',system_dependent('getwinsys')]);
            catch
                osversion = 'Unknown';
            end
        else
            try
                [status, osversion] = system('uname -s');
                osversion = deblank(osversion);
            catch
                osversion = 'Unknown';
            end            
        end
        
        
        if isempty(osversion)
           osversion = 'Unknown';
        end
        
        % Define machine architecture to be Intel.
        arch = 'Intel';
        % Define path to our own netcat executable for M$-Windows:
        nccommand = strcat('"',[PsychtoolboxRoot 'PsychContributed\nc '],'"');
    end

    if IsLinux
        ostype = 'LinuxOS';
        [rc osversion] = system('uname -r');
        osversion=deblank(osversion);
        % Query machine architecture via Unix 'arch' utility.
        [rc arch] = system('uname -m');
        arch=deblank(arch);
    end
    
    % Query runtime environment:
    if IsOctave
        runtimeenv = 'Octave';
        runtimeversion = version;
    else
        runtimeenv = 'Matlab';
        runtimeversion = version;
    end

    % Query what Matlab thinks as well:
    runtimearch = computer;

    % Query MAC address as unique machine id:
    % We try to get the info from the 'Computer' subfunction of Screen:
    if isfield(compinfo, 'MACAddress')
        % Success. Use it.
        mac = compinfo.MACAddress;
    else
        % Failed: Try harder...
        if IsWin
            % Use external helper-utility to try to query MAC address:
            macidcom = [PsychtoolboxRoot 'PsychContributed\macid '];
            [rc mac] = dos(macidcom);
            if rc==0 & length(mac)>=12 %#ok<AND2>
                % Worked on Windows: Reassemble MAC into our standard
                % format:
                mac = [mac(1) mac(2) ':' mac(3) mac(4) ':' mac(5) mac(6) ':' mac(7) mac(8) ':' mac(9) mac(10) ':' mac(11) mac(12)];
            else
                % Failed as well. Set to unknown default.
                mac = '00:00:00:00:00:00';
            end
        else
            % On Non-Windows system, we are out of luck and use our
            % 'unknown' default:
            mac = '00:00:00:00:00:00';
        end
    end

    % Build unique id string for this system:
    uniqueID = ['<MACID>' mac '</MACID><OS>' ostype '-' osversion '</OS><ENVIRONMENT>' runtimeenv '</ENVIRONMENT><ENVVERSION>' ...
                runtimeversion '</ENVVERSION><ENVARCH>' runtimearch '</ENVARCH><CPUARCH>' arch '</CPUARCH><FLAVOR>' ...
                flavor '</FLAVOR><ISUPDATE>' num2str(isUpdate) '</ISUPDATE><DATE>' date '</DATE>'];
    
    fprintf('Online Registration: Will try to transmit the following string of data\n');
    fprintf('to the www.psychtoolbox.org website for statistical purpose:\n\n');
    fprintf('%s\n\n', uniqueID);
    fprintf('Type ''help PsychtoolboxRegistration'' at the Matlab/Octave command prompt\n');
    fprintf('to learn about the purpose and scope of online registration.\n');
    fprintf('Type ''type PsychtoolboxRegistration'' to see the source code of this routine.\n\n');
    fprintf('Data transfer can take up to 10 seconds... The system reports:\n');

    %if IsOctave
    % MK: Octave now supports pnet, so we can disable this code branch:
    if 0
        % pnet not yet supported on Octave. Use netcat as in good ol' days:

        % Execute transmission command: We time out after 10 seconds if it does not work.
        ptbserveraddress = [ptbserveraddress ' 2000'];
        syscmd = ['echo "' uniqueID '" | ' nccommand ' -w 10 -v ' ptbserveraddress ' '];

        fflush(stdout);
        
        if IsWin
            rc = dos(syscmd);
        else
            rc = system(syscmd);
        end
        fprintf('\n');
        
        fflush(stdout);
    else
        % pnet supported: Use that...
        % Specifically that means we will always use pnet() on MS-Windows,
        % as we don't support Octave there. This will allow us to get rid
        % of distributing our own nc netcat command, which makes trouble
        % for some users with really idiotic configurations of virus
        % scanners and incompetent IT staff.
        %
        psychlasterror('reset');
        rc = 0;

        con=pnet('tcpconnect', ptbserveraddress, 2000);
        if con >= 0
            % Connection established.
            % Write our string, with a timeout of 10 seconds:
            pnet(con, 'setwritetimeout', 10);

            try
                % This try-catch is just to work around a bug in
                % pnet('printf'), pretty annoying.
                pnet(con, 'printf', '%s\n', uniqueID);
            catch
                psychlasterror('reset');
            end
        else
            % Failed!
            rc = 1;
        end

        pnet('closeall');
    end
    
    % Did it work?
    if rc==0
        fprintf('Success! Thanks for online registration of this copy.\n');
    else
        fprintf('Failed! Anyway, this does not affect your use of PTB in any way,\n');
        fprintf('only our statistics about usage will be inaccurate :(\n');
    end
catch
    % This empty try-catch-end framework is just to make absolutely sure that our
    % installation procedure does not fail due to some error in the online registration
    % script.
end

% We are done.
return;
