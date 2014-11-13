###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################


include ../../debug.mk
CC = arm-linux-gcc
AR = arm-linux-ar
STRIP = arm-linux-strip
KERNEL = ../../../../buildsystem/linux-2.6.17.14/
#KERNEL = /root/work/babymonitor/sources/buildsystem/linux-2.6.17.14/
OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

MLS_HEADERS = mlsErrors.h mlsTypes.h mlsInclude.h mlsicamConfig.h mlstrigger.h
#CX93510.h OV7675.h

#CFLAGS += -O0 -g3 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC -I/home/mlslnx003/icambuild/linux_n_fs/linux-2.6.25/include
ifeq ($(DEBUG_HOST),pcd)
CFLAGS += -lm -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-crossjumping -falign-functions -DLINUX -D_GNU_SOURCE -Wall -fPIC -I $(KERNEL)include  -I ../
else
CFLAGS += -lm -O2 -DLINUX -D_GNU_SOURCE -Wall -fPIC -I $(KERNEL)include  -I ../
endif

ifeq ($(DEBUG_MEM),DMALLOC)
CFLAGS += -I../../pcd/include -DDMALLOC -DDMALOC_FUNC_CHECK
else

endif

LDFLAGS = -lpthread


#CFLAGS += -DDEBUG -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
LFLAGS += -lpthread -ldl
OBJECTS = input_mlsicam.o mlsWifi.o mlsimage.o mlsjpeg.o mlssensor.o mlsicam.o mlsaudio.o mlsio.o mlsflash.o mlsVox.o mlsAuthen.o mlsOTA.o sha2.o mlstrigger.o VoiceFilterBank_msc3.o  step_motor.o ping.o minIni.o postoffice.o sys_configs.o nxcCRC32.o

all: input_mlsicam.a

clean:
	rm -f *.a *.o core *~ *.so *.lo

input_mlsicam.a: $(OBJECTS)
	$(AR) rcs $@ $^
%c.%o:
	$(CC) -c $(CFLAGS) -o $@ $<

	
	
