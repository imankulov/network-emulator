---
layout: default
title: About network-emulator
---

About
---------

Network-emulator is a simple utility intended to test how network losses and
delays affect speech quality in VoIP-based applications.  With this tool
experimenter can set up loss rate, bandwidth, encoder options and select one of
the packet loss suppression algorithm.

Emulator can help quickly obtain these measures:

- compare encoding quality for different codecs and codecs modes;
- estimate the impact of the loss level and distribution on the speech
  quality;
- estimate the impact of the different PLC algorithms on the speech quality.

Quality estimation may be carried out subjectively but the better way is to use
automatic tests such as PESQ (reference implementation can be found at [ITU-T
official site][1]). Network emulator only helps to obtain reference and
degraded speech samples.

[1]: http://www.itu.int/rec/T-REC-P.862/en "P.862"

