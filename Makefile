#Add -DFORCE_PC3_IDENT to CFLAGS to force the identification of
#a Parallel Cable III
CFLAGS=-Wall -fPIC -DUSB_DRIVER_VERSION="\"$(shell stat -c '%y' usb-driver.c |cut -d\. -f1)\"" #-DFORCE_PC3_IDENT

LIBS=-ldl -lusb -lpthread

SRC=usb-driver.c parport.c config.c jtagmon.c
HEADER=usb-driver.h parport.h jtagkey.h config.h jtagmon.h

ifeq ($(LIBVER),32)
CFLAGS += -m32
endif

FTDI := $(shell libftdi-config --libs 2>/dev/null)
ifneq ($(FTDI),)
SRC += jtagkey.c
CFLAGS += -DJTAGKEY
LIBS += $(FTDI)
endif

SOBJECTS=libusb-driver.so libusb-driver-DEBUG.so

all: $(SOBJECTS)
	@file libusb-driver.so | grep x86-64 >/dev/null && echo Built library is 64 bit. Run \`make lib32\' to build a 32 bit version || true

libusb-driver.so: $(SRC) $(HEADER) Makefile
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LIBS) -shared

libusb-driver-DEBUG.so: $(SRC) $(HEADER) Makefile
	$(CC) -DDEBUG $(CFLAGS) $(SRC) -o $@ $(LIBS) -shared

lib32:
	$(MAKE) LIBVER=32 clean all

clean:
	rm -f $(SOBJECTS)

.PHONY: clean all lib32
