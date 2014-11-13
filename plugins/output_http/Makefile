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

OTHER_HEADERS = ../../mjpg_streamer.h ../../utils.h ../output.h ../input.h

#CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -DMEMWATCH -Wall -shared -fPIC -I. -I..

ifeq ($(DEBUG_HOST),pcd)
CFLAGS += -lm -g -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-strict-aliasing -fno-crossjumping -falign-functions  -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC -I $(KERNEL)include  -I ../ -I.
else
CFLAGS += -O2 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC -I. -I..
endif

ifeq ($(DEBUG_MEM),DMALLOC)
CFLAGS += -I../../pcd/include -DDMALLOC -DDMALOC_FUNC_CHECK
else

endif


#CFLAGS += -O0 -g3 -DLINUX -D_GNU_SOURCE -Wall -shared -fPIC
#CFLAGS += -DDEBUG
LFLAGS += -lpthread -ldl
#OBJECTS = output_http.o httpd.o memwatch.o
OBJECTS = output_http.o httpd.o url_encode.o
all: output_http.a

clean:
	rm -f *.a *.o core *~ *.so *.lo

output_http.a: $(OBJECTS)
	$(AR) rcs $@ $^
%c.%o:
	$(CC) -c $(CFLAGS) -o $@ $<