#!/bin/sh

g++ -Ofast -mfpu=neon -pthread -std=c++11 -DVOLUMIO=1 -DVOLUMIO_QUEUE -DENABLE_GETOPT mpd_gui.cpp json11.cpp -o mpd_gui.cpp.o_nanohatoled `pkg-config --cflags opencv` `pkg-config --libs opencv` `freetype-config --cflags` `freetype-config --libs` `taglib-config --cflags` `taglib-config --libs` -lcurl -DDISPLAY_NANOHATOLED
g++ -Ofast -mfpu=neon -pthread -std=c++11 -DVOLUMIO=1 -DVOLUMIO_QUEUE -DENABLE_GETOPT mpd_gui.cpp json11.cpp -o mpd_gui.cpp.o_ips240240 `pkg-config --cflags opencv` `pkg-config --libs opencv` `freetype-config --cflags` `freetype-config --libs` `taglib-config --cflags` `taglib-config --libs` -lcurl -DDISPLAY_13IPS240240


