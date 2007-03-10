CFLAGS=-Wall

all: libusb-driver.so libusb-driver-DEBUG.so

libusb-driver.so: usb-driver.c usb-driver.h
	gcc -fPIC $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

libusb-driver-DEBUG.so: usb-driver.c usb-driver.h
	gcc -fPIC -DDEBUG $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

clean:
	rm -f libusb-driver.so libusb-driver-DEBUG.so

.PHONY: clean all
