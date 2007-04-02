#Add -DFORCE_PC3_IDENT to CFLAGS to force the identification of
#a Parallel Cable III
CFLAGS=-Wall -fPIC #-DFORCE_PC3_IDENT

SOBJECTS=libusb-driver.so libusb-driver-DEBUG.so

all: $(SOBJECTS)

libusb-driver.so: usb-driver.c usb-driver.h Makefile
	gcc $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

libusb-driver-DEBUG.so: usb-driver.c usb-driver.h Makefile
	gcc -DDEBUG $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

clean:
	rm -f $(SOBJECTS)

.PHONY: clean all
