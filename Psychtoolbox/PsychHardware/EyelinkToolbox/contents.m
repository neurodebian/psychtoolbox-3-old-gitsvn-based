% 	EyelinkToolbox.
% 	Version 1.4.4 		      27-11-02
% 
% 	The EyelinkToolbox is a collection of m-files and a Mex file that
%	can be used to control the SR research-Eyelink� gazetracker.
%
%   EYELINK is a MEX/DLL file, which can be used as an interface between the eyelink
%   and matlab. The EyelinkToolbox uses the same approach as the SCREEN mex/dll function
%	provided in the PsychToolBox (http://www.psychtoolbox.org/) and also uses the
%	functions provided by this toolbox for doing any graphics involved (so you need to
%	install the PsychToolbox for the EyelinkToolbox to function properly).
%
%	We greatly appreciate the effort David Brainard and Denis Pelli have undertaken
%	in making the PsychToolBox. We have further benefitted from the experience of
% 	Jos van der Geest and Maarten Frens.
%
%	Note that this toolbox is NOT provided nor endorsed by SR-Research
%	(http://www.eyelinkinfo.com/), the supplier of the Eyelink gazetracker, so do not contact
%	them about this. Rather, post questions on the PsychToolbox mailinglist, or directly to us.
%	If you're interested in the c-source code of this project, we will be
%	glad to send it to you.
%	
%	Disclaimer: we cannot be hold responsible for any damage that may (appear to) be
%				caused by the use of this toolbox. Use at your own risc.
%
%   Most of the Eyelink functions that are avaible in C are now also available
%	under Matlab. If any particular function you need is still missing, let us know
%	and we'll try to incorporate it into a next release. In case you decide to add
%	or modify functions yourself, please send us the modified code.
% 	If you think you've found a bug, please tell us: f.w.cornelissen@med.rug.nl
% 	It will help greatly if you can supply a  minimal-length program that exhibits 
% 	the bug.
%
%   For a complete list of available functions type "EYELINK" in the matlab command window
%   For an explanation of any particular eyelink function just add a
%   question mark "?" after a command.
% 	E.g. for 'Initialize', try either of these equivalent forms:
% 		EYELINK('Initialize?')
% 		EYELINK initialize?
%
% 	[optional arguments]:
% 	Brackets in the function list, e.g. [remport], indicate optional arguments, not
% 	matrices. Optional arguments must be in order, without omitting earlier ones.
% 	
%
%	Contents of the EyelinkToolbox folder:
%	-Contents.m: this file.
%	-Changes.m: documents changes
%	-EyelinkBasic
%		EYELINK.mex file and a collection of m-files
%	-EyelinkDemos
%		-Short demo: file 'eyelinkexample.m'
%						Shows a simple gaze cursor. Blinking erases the screen.
%						simple demo program to illustrating the use of this 
%						toolbox. Note that it uses the 'dotrackersetup' contained
%						within the mex file, rather then the m-file based version.
%		-EyelinkDemoExperiment: 'eyelinkdemoexp.m': a simple demo experiment
%						with the flavour of the c-coded demo experiment of Dave Stampe '97.
%						there are two (types of) trials:
%							simpletrialdemo: a simple gaze recording example
%							realtimedemo: a simple gaze-dependent display example
%		-Palmer demo: Simple experiment to measure response time using eye movements.
%
%	-EyelinkSounds: a few sounds that you can add to your system to recreate
%						that particular Eyelink Mac-experience.
%						On a Mac, add the sounds to your system by dragging
%						the sound files this folder onto your closed system folder.
%						You can't have any programs open when copying sound
%						resources to your system.
%						use 'testeyelinksounds' to test for the presence of all required sounds
%
%	-EyelinkTests: 
%			-testgetkeyforeyelink.m : 
%						Function that tests the getkeyforeyelink routine and
%						computes the time taken by the getkeyforeyelink function call
%			-testcalls.m
%						Program that tests if most of the eyelink routines are operational, 
%						also useful as illustration of the eyelink function calls(no calibration).
%			-testcalib.m :
%						Tests and illustrates the eyelink calibration routine.
%						
%			-testsampletime.m :
%						Program which performs multiple timing-tests on functions related
%						to sampling eye position.
% 			-EXGetEyeLinkTime.m
% 						tests the new requesttime and readtime functions.					
% 			-testbutton.m
% 						test of the new buttonstates and lastbuttonpress functions. 
%
%	Some things to observe before using the toolbox:
% 	*Software used to produce and test the code. 
% 	Macintosh version
%		Mac OS 9.1.
%		C-compiler: CodeWarrior 6.1.
%		Matlab 5.2.1
%		Operator PC: EyeLink 2.04
%		PsychToolbox 2.5.2 + screen 2.5.3
%
% 	PC version
% 		Windows 98 & XP (XP tested in dummy mode)
% 		C-compiler: MS Visual C
% 		Matlab 6.1
% 		PsychToolbox 2.5.0
% 
%  *The EyelinkToolbox folder can be put anywhere on your harddisk as long as you make
%	sure that Matlab knows about it's existence. So use the 'set path' command under the
%	file menu. You only need to add the 'EyelinkToolbox' folder
%	*The sounds the EYELINK toolbox uses as well as many other defaults need
% 	to be included in your system folder, otherwise (some of) the demo's will stop.
% 	Use 'testeyelinksounds.m' to see if the necessary sounds are present
%	*Currently, for the mac, there are both m-file as well as mex-file implementations of dotrackersetup,
%	and dodriftcorrection. If there are no problems with timing, it is our
%	idea to enable only the m-file versions in a more final release. The great advantage
%	of using only m-files is that this makes the code much more portable and flexible to use.
%	*we have not done any exhaustive testing on the timing of this toolbox, so do not
%	assume that things are as they used to be with the pure c-code versions of your experiments.
%	*The current version is Powermac only. While we do have the intention to port the mex-code to
%	the windows version, this currently has no great priority for us.
%	We welcome any observations, suggestions that may help us improve this toolbox.
%
%	Enno Peters, Frans Cornelissen and John Palmer
%	Groningen, 27-11-2002
%	email: f.w.cornelissen@med.rug.nl
%
% 	Copyright (c) 2001, 2002 by Laboratory of Experimental Ophthalmology, University of Groningen
%
%
%
%
%
%
