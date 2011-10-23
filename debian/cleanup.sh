#!/bin/bash
#emacs: -*- mode: shell-script; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- 
#ex: set sts=4 ts=4 sw=4 noet:
set -eu

RM="git rm --ignore-unmatch"
FINDREGEX="find -regextype posix-egrep -regex"

{ $FINDREGEX '.*\.(o|dll.*|exe|mex.*|dylib|class)' -print0; \
  $FINDREGEX '.*\.(old|bak).*' -print0; } | xargs -0 --no-run-if-empty $RM -rf
$FINDREGEX '.*((|Mac)OSX|Octave3OSXFiles|Windows|.bundle|.FBCLockFolder)' -print0| xargs -0 --no-run-if-empty $RM -rf
# more of non-Linux OS specifics
$RM -rf PsychSourceGL/Cohorts/libusb1-win32
$RM -rf PsychSourceGL/Cohorts/{PsychtoolboxOSXKernelDriver,HID\ Utilities\ Source,IOWarrior}
$RM -rf Psychtoolbox/PsychContributed/WinTab
$RM -rf Psychtoolbox/PsychAlpha/PsychtoolboxKernelDriver.kext*
$RM -rf Psychtoolbox/PsychAlpha/PsychtoolboxKernelDriverUserClientTool

# Test and its data (input? output?)
$RM -f Psychtoolbox/PsychTests/StandaloneTimingProgram Psychtoolbox/.FBCIndex
# ???
# Psychtoolbox/PsychTests/StandaloneTimingTest.m refers StandaloneTimingTest.c which is N/A

# some additional externals: use system ones
$RM -f ./PsychSourceGL/Cohorts/Kinect-* \
      ./Psychtoolbox/PsychContributed/Kinect-*-Windows*

# Binary built against ATI/AMD SDK with questionable
# redistributability
$RM -f Psychtoolbox/PsychContributed/ATIRadeonperf_Linux

# __TODO:
# http://docs.psychtoolbox.org/DrawTextPlugin
# BDepends: libfontconfig1-dev, libfreetype6-dev
# uses http://sourceforge.net/projects/oglft/  with last release from 2003 :-/
# not pruning for now
# built for i386 so needs following Depends ATM:
# ia32-libs, lib32gcc1, lib32z1, libc6-i386, zlib1g
$RM -f ./Psychtoolbox/PsychBasic/PsychPlugins/libptbdrawtext_ftgl*.*

# TODO: build
# $RM -rf ./PsychSourceGL/Cohorts/FTGLTextRenderer # libftgl-dev -- check ver
# BDepends: libfontconfig1-dev, libfreetype6-dev
# g++ -g -I. -I/usr/include/ -I/usr/include/freetype2/ -L/usr/lib -l GL -l GLU -l fontconfig -l freetype -pie -shared -fPIC -o libptbdrawtext_ftgl.so.1 libptbdrawtext_ftgl.cpp qstringqcharemulation.cpp OGLFT.cpp


# Windows only
$RM -rf Psychtoolbox/PsychContributed/macidpascalsource

# Docs without sources... may be  into non-free?
$RM -f ./Psychtoolbox/PsychDocumentation/{ECVP2010Poster,Psychtoolbox3-Slides}*.pdf

# _TODO:
# Remove things which are said to be superseeded and NA for Linux
# See Psychtoolbox/PsychDocumentation/ExperimentalStuff.m
# ??? PsychSound
# ??? ./Psychtoolbox/PsychHardware/EyelinkToolbox -- N/A used under Linux

## EXTERNALS
# prune some externals present in Debian:
$RM -rf ./PsychSourceGL/Cohorts/libDC1394 # libdc1394-22-dev -- check content

# prune copies of GLEW
$RM -f ./Psychtoolbox/PsychOpenGL/MOGL/source/*gl*ew.*
$RM -f ./PsychSourceGL/Source/Common/Screen/gl*ew*.*

# TODO:
# portaudio has to be patched - keep original pristine tarball and patched sources
# remove the rest
$RM -f ./PsychSourceGL/Cohorts/PortAudio/libportaudio.a # anyway alien arch
$RM -f ./PsychSourceGL/Cohorts/PortAudio/portaudio_unpatched_except4OSX.zip # we will use pristine source tarball
$RM -rf ./PsychSourceGL/Source/Common/PsychSound/StaticOpenALLib # libopenal-dev -- check


# artoolkit (RFP - #452233) -- needed only on OSX/Windows
$RM -rf PsychSourceGL/Cohorts/ARToolkit
$RM -rf PsychSourceGL/Projects/Linux/ARToolkit
$RM -rf Psychtoolbox/PsychDemos/ARToolkitDemo*

# _TODO:
# Conside packaging MOGL: MOGL OpenGL for Matlab support
# $RM -rf ./Psychtoolbox/PsychOpenGL/MOGL

# TODO:
# IViewXToolbox: ./Psychtoolbox/PsychHardware/iViewXToolbox
# uses tcp_udp_ip toolbox
# Strip copy of 1.0 version of the toolbox
$RM -rf ./Psychtoolbox/PsychHardware/iViewXToolbox/tcp_udp_ip/tcpip*

## Additional Dependencies:
## OSX: Cocoa/Cocoa.h used in StoreBitLib_Prefix.pch
## Check more on compatibility with Linux of components under Psychtoolbox/PsychHardware

#
# Some final look
#

# to find more of non-sources and report them:
echo "I: Various non-sources/data"
find -regextype posix-egrep -type f ! -regex '.*\.(m(|at)|dat|c(|c|pp)|h|java|sh|txt|in|rtf|py|php|wav|bmp|png|jpg|JPG|tiff|html|mov|xml)' -ls | grep -v -e '\.git' -e '\.pc'

# Look among executables:
echo "I: Suspecious executables left"
find -perm /+x ! -type d -print0 | xargs -0 file | grep -v -e ASCII -e '\.git' -e 'shell' -e text -e JPEG -e 'Rich Text'

echo "I: Leftout archives"
$FINDREGEX '.*\.(zip|rar)'

echo "I: Leftout static libs"
find -iname lib*.a
