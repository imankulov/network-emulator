About
-------
Network-emulator is a simple utility intended to test how network losses
affects speech quality in VoIP-based applications. Experimenter can set up loss
rate, bandwidth, encoder options and select one of the packet loss suppression
algorithm.

In fact this tool is quite small and extensively uses the power of the [PJSIP
library][1].

Emulator can help quickly obtain these measures:

 - compare encoding quality for different codecs and codecs modes.
 - estimate the impact of the loss level and distribution on the speech
   quality.
 - estimate the impact of the different PLC algorithms on the speech quality.

Quality estimation may be carried out subjectively but better way is to use
automatic tests such as PESQ (reference implementation can be found at [ITU-T
official site][0]). Network emulator only helps to obtain reference and
degraded speech samples.

Compiling
-----------

First before you need to download and build [PJSIP library][1]. To use some
proprietary codecs as G.729 and/or G.723.1 follow [these instructions][2].

It seems that you already have downloaded this ulility from [network emulator
github repository][3].

After that set up enviromenent variable `PJBASE` in order to specify location of
the PJSIP library or run `make` command with this variable set up:

    make PJBASE=... all

PJSIP libraries are compiled in statically, so in order to deploy or use just
built application you don't need to modify your `LD_LIBRARY_PATH` or include
some parts of PJSIP in the distributed package. However Intel IPP codecs usage
requires correctly adjusted environment.

Command-line options
-----------------------

 - `-i|--input-file <filename1.wav>` -- path to input (reference) file
 - `-o|--output-file <filename2.wav>` -- path to output (degraded) file
 - `-c|--codec <CODEC_NAME>` -- codec name, i.e. speex/8000  or G729
 - `-l|--loss <lost_pct>` -- loss rate (float, %)
 - `--p00 <lost_pct>` -- p00 (lost probability when previous packet was lost, float, %)
 - `--p10 <lost_pct>` -- p10 (lost probability when previous packet was received, float, %)
 - `-f|--fpp <fpp>` -- packetization coefficient (number of codec frames per one RTP packet)
 - `-p|--plc empty|repeat|smart|noise` -- PLC algorithm (see below)
 - `-q|--speex-quality <value>` -- Speex quality (0-10) (works with speex algorithm only obviously)
 - `   --log-level <0..6>` -- Log level where 0 means "log nothing" and 6 means  "log everything"

This list can be not exhaustive.  In order to obtain more comprehensive help
refer to emulator(1) man page.


Packet loss concealment algorithms
------------------------------------

There is a number of packet loss concealment algorithms.

 - empty -- no PLC, lost frames replaced with empty ones
 - repeat -- lost frames replaced wih last received frame
 - smart -- "smart" PLC, based on [WSOLA][4] algorithm or (for Speex) built-in
 speex PLC methods
 - noise -- replace lost frames with white noise

[0]: http://www.itu.int/rec/T-REC-P.862/en "PESQ"
[1]: http://www.pjsip.org
[2]: http://trac.pjsip.org/repos/wiki/Intel_IPP_Codecs "Using Intel IPP with PJMEDIA"
[3]: http://github.com/imankulov/network-emulator/tarball/master "Network emulator tarball"
[4]: http://www.pjsip.org/pjmedia/docs/html/group__PJMED__WSOLA.htm "WSOLA"
