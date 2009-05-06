About
-------
Network-emulator is a simple utility intended to test how network losses
affects speech quality in VoIP-based applications. Experimenter can set up loss
rate, codec and select one of the packet loss suppression algorithm.

In fact this tool is quite small and extensively uses the power of the [PJSIP
library][1].

Compiling
-----------

First before you need to download and build [PJSIP library][1]. To use some
proprietary codecs as G.729 and/or G.723.1 follow [these instructions][2].

After that set up enviromenent variable `PJBASE` in order to specify location of
the PJSIP library or run `make` command with this variable set up:

    make PJBASE=... all

PJSIP libraries are compiled in statically, so in order to deploy or use just
built application you don't need to modify your `LD_LIBRARY_PATH` or include
some parts of PJSIP in the distributed package. However Intel IPP codecs usage
requires correctly adjusted environement.

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


Packet loss concealment algorithms
------------------------------------

There is a number of packet loss concealment algorithms.

 - empty -- no PLC, lost frames replaced with empty ones
 - repeat -- lost frames replaced wih last received frame
 - smart -- "smart" PLC, based on [WSOLA][3] algorithm or (for Speex) built-in
 speex PLC methods
 - noise -- replace lost frames with white noise


[1]: http://www.pjsip.org
[2]: http://trac.pjsip.org/repos/wiki/Intel_IPP_Codecs "Using Intel IPP with PJMEDIA"
[3]: http://www.pjsip.org/pjmedia/docs/html/group__PJMED__WSOLA.htm "WSOLA"
