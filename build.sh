#!/bin/sh

make clean
make DISPTYPE=-DDISPLAY_NANOHATOLED TARGET=mpd_gui.cpp.o_nanohatoled

make clean
make DISPTYPE=-DDISPLAY_13IPS240240 TARGET=mpd_gui.cpp.o_ips240240



