CFLAGS:=
CPP=g++

SIO_PATH=/home/volumio/socket.io-client-cpp/src/

CFLAGS+= -Ofast -mfpu=neon -pthread -std=c++11
CFLAGS+= -DVOLUMIO=1 -DENABLE_GETOPT

CFLAGS+=	`taglib-config --cflags`
LIBS+=		`taglib-config --libs`
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


