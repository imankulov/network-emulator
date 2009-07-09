---
layout: default
title: Build network-emulator 
---

Download and build instructions
--------------------------------

Latest network emulator can be downloaded from github. You can either download it
with tarball or clone git reposltory.

Clone:

    $ git clone git://github.com/imankulov/network-emulator

Latest [tarball][1] or [zipped file][2] can be downloaded with given links.

In order to build network emulator from source it's neсessary to download and
build [PJSIP library][3]. To use some proprietary
codecs as G.729 and/or G.723.1 one must follow [the instructions from PJSIP
project][4].

After PJSIP being downloaded and built you should set up enviromenent variable
`PJBASE` in order to specify location of this library. Either you can run
`make` command with this variable set up::

    make PJBASE=... all

PJSIP libraries are compiled in statically, so in order to deploy or use just
built application you don't need to modify your `LD_LIBRARY_PATH` or include
some parts of PJSIP in the distributed package. However Intel IPP codecs usage
requires correctly adjusted environment.

[1]: http://github.com/imankulov/network-emulator/tarball/master "network-emulator tarball"
[2]: http://github.com/imankulov/network-emulator/zipball/master "network-emulator zip-file"
[3]: http://www.pjsip.org "PJSIP library"
[4]: http://trac.pjsip.org/repos/wiki/Intel_IPP_Codecs "Intel IPP codecs"
