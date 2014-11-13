###############################################################
#
# Purpose: Makefile for "M-JPEG Streamer"
# Author.: Tom Stoeveken (TST)
# Version: 0.3
# License: GPL
#
###############################################################

include debug.mk

KERNEL = ../../buildsystem/linux-2.6.17.14/
#KERNEL = /root/work/babymonitor/sources/buildsystem/linux-2.6.17.14/
TFTP = /tftpboot
CC = arm-linux-gcc
STRIP = arm-linux-strip
CFLAGS += -DLINUX -D_GNU_SOURCE -Wall -I $(KERNEL)include -I.
LFLAGS += -lpthread -lm
SFLAGS = plugins/input_mlsicam/input_mlsicam.a plugins/output_http/output_http.a 
ifeq ($(DEBUG_HOST),pcd)
CFLAGS +=  -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-crossjumping -falign-functions -DDEBUG_PCD -I./pcd/include
LFLAGS += -L./pcd -lipc -lpcd
else
CFLAGS += -O2
endif

ifeq ($(DEBUG_MEM),DMALLOC)
CFLAGS += -DDMALLOC -DDMALLOC_FUNC_CHECK
SFLAGS += ./pcd/libdmallocth.a
else
endif

#CFLAGS += -O0 -g3 -DLINUX -D_GNU_SOURCE -Wall
#CFLAGS += -O2 -DDEBUG -DLINUX -D_GNU_SOURCE -Wall


APP_BINARY=mjpg_streamer_icam
OBJECTS=mjpg_streamer.o utils.o debuglog.o

all: application
#	cp ./plugins/input_mlsicam/input_mlsicam.so $(TFTP)
#	cp ./plugins/output_http/output_http.so $(TFTP)
	cp ./mjpg_streamer_icam $(TFTP)
clean:
#	make -C plugins/input_uvc $@
	make -C plugins/input_mlsicam $@
#	make -C plugins/input_testpicture $@
#	make -C plugins/output_file $@
	make -C plugins/output_http $@
#	make -C plugins/output_autofocus $@
#	make -C plugins/input_gspcav1 $@
	rm -f *.a *.o $(APP_BINARY) core *~ *.so *.lo

plugins: input_mlsicam.a output_http.a
# input_testpicture.so output_autofocus.so input_gspcav1.so output_file.so

application: plugins $(APP_BINARY)

input_mlsicam.a:
	make -C plugins/input_mlsicam all
output_http.a:
	make -C plugins/output_http all


$(APP_BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ $(SFLAGS)
	$(STRIP) $(APP_BINARY)
	chmod 755 $(APP_BINARY)
%c.%o:
	$(CC) -c $(CFLAGS) -o $@ $<
# useful to make a backup "make tgz"
tgz: clean
	mkdir -p backups
	tar czvf ./backups/mjpg_streamer_`date +"%Y_%m_%d_%H.%M.%S"`.tgz --exclude backups *
