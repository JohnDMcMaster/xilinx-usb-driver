#Add -DFORCE_PC3_IDENT to CFLAGS to force the identification of
#a Parallel Cable III
CFLAGS=-Wall -fPIC #-DFORCE_PC3_IDENT

FTDI := $(shell libftdi-config --libs 2>/dev/null)
ifneq ($(FTDI),)
JTAGKEYSRC = jtagkey.c
CFLAGS += -DJTAGKEY
endif

SOBJECTS=libusb-driver.so libusb-driver-DEBUG.so

all: $(SOBJECTS)

libusb-driver.so: usb-driver.c parport.c jtagkey.c config.c usb-driver.h parport.h jtagkey.h config.h Makefile
	gcc $(CFLAGS) usb-driver.c parport.c config.c $(JTAGKEYSRC) -o $@ -ldl -lusb -lpthread $(FTDI) -shared

libusb-driver-DEBUG.so: usb-driver.c parport.c jtagkey.c config.c usb-driver.h parport.h jtagkey.h config.h Makefile
	gcc -DDEBUG $(CFLAGS) usb-driver.c parport.c config.c $(JTAGKEYSRC) -o $@ -ldl -lusb -lpthread $(FTDI) -shared

clean:
	rm -f $(SOBJECTS)

.PHONY: clean all
