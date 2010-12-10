#!/bin/bash
#emacs: -*- mode: shell-script; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*- 
#ex: set sts=4 ts=4 sw=4 noet:
#-------------------------- =+- Shell script -+= --------------------------
#
# @file      cleanup.sh
# @date      Thu Dec  9 21:27:26 2010
# @brief
#
#
#  Yaroslav Halchenko                                            Dartmouth
#  web:     http://www.onerussian.com                              College
#  e-mail:  yoh@onerussian.com                              ICQ#: 60653192
#
# DESCRIPTION (NOTES):
#
# COPYRIGHT: Yaroslav Halchenko 2010
#
# LICENSE: MIT
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.
#
#-----------------\____________________________________/------------------


find -regextype posix-egrep -regex '.*\.(a|dll|exe|mex.*|dylib)' -delete
find -regextype posix-egrep -regex '.*((|Mac)OSX|Windows|.bundle)' | xargs rm -rf
# more of non-Linux OS specifics
rm -rf PsychSourceGL/Cohorts/{PsychtoolboxOSXKernelDriver,HID\ Utilities\ Source,IOWarrior}
rm -rf Psychtoolbox/PsychContributed/WinTab
rm -rf Psychtoolbox/PsychAlpha/PsychtoolboxKernelDriver.kext*

# some additional externals not yet "in play" or might be again
# non-Linux OS specific
rm -f ./PsychSourceGL/Cohorts/Kinect-* \
      ./Psychtoolbox/PsychContributed/Kinect-*-Windows*

# Docs without sources... may be  into non-free?
rm -f ./Psychtoolbox/PsychDocumentation/{ECVP2010Poster,Psychtoolbox3-Slides}*.pdf

## EXTERNALS
# prune some externals present in Debian:
rm -rf ./PsychSourceGL/Cohorts/libDC1394 # libdc1394-22-dev -- check content
# portaudio was said to be patched,
# portaudio_unpatched_except4OSX.zip contained originals
#pa_front.c:#define PA_VERSION_  1899
#pa_front.c:#define PA_VERSION_TEXT_ "PortAudio V19-devel WITH-DIM"
#$Id: pa_asio.cpp 1097 2006-08-26 08:27:53Z rossb $
# pa_asio.cpp - patched, new function and tune-up
# pa_unix_util.c -- patch , conditioning on MK_PSYCH_RTSCHED
# pa_front.c, pa_process.c  - pristine
rm -rf ./PsychSourceGL/Cohorts/PortAudio # libportaudio-dev -- check ver
rm -rf ./PsychSourceGL/Cohorts/FTGLTextRenderer # libftgl-dev -- check ver
rm -rf ./PsychSourceGL/Source/Common/PsychSound/StaticOpenALLib # libopenal-dev -- check
# What is that one exactly is?
rm -f ./Psychtoolbox/PsychBasic/PsychPlugins/libptbdrawtext_ftgl.so.1

# To package:
# artoolkit (RFP - #452233)
rm -rf PsychSourceGL/Cohorts/ARToolkit

## Additional Dependencies:
## Cocoa/Cocoa.h used in StoreBitLib_Prefix.pch
## Check on compatibility with Linux of components under Psychtoolbox/PsychHardware

# to find more of non-sources
find -regextype posix-egrep -type f ! -regex '.*\.(m|c(|c|pp)|h|txt|in)' -ls | grep -v '\.git'
