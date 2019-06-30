DISPTYPE:= -DDISPLAY_13IPS240240
CFLAGS:=
CFLAGS+= $(DISPTYPE)
CFLAGS+= -DDTBSELECTION -DDTBSELECTION_LIST=\"/boot/dac_dtbs.txt\" -DDTBSELECTION_COPYTO=\"sun50i-h5-nanopi-neo2.dtb\"
CFLAGS+= -DGUISELECTION -DGUISELECTION_LIST=\"/boot/guilist.txt\"

CPP=g++

SIO_PATH=/home/volumio/socket.io-client-cpp/src/

CFLAGS+= -Ofast -mfpu=neon -pthread -std=c++11
CFLAGS+= -DVOLUMIO=1 -DENABLE_GETOPT

CFLAGS+=	`pkg-config --cflags opencv` \
			`freetype-config --cflags` \
			`taglib-config --cflags`
LIBS+=		`pkg-config --libs opencv` \
			`freetype-config --libs` \
			`taglib-config --libs`
LIBS+= -lcurl -lboost_system -lpthread

# socket.io-client-cpp
CFLAGS+= -I $(SIO_PATH)
LIBS+= -L $(SIO_PATH) -lsioclient

# sources
OBJS+= mpd_gui_sio.o
OBJS+= MusicControllerVolumioSIO.o
OBJS+= MenuController.o

TARGET:=mpdguing

.SUFFIXES: .cpp .o

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CPP) $^ $(LIBS) -o $(TARGET) 

.cpp.o:
	$(CPP) $(CFLAGS) -c $<

.PHONY:clean

clean:
	$(RM) $(TARGET) $(OBJS)

