CFLAGS=-Wall -fPIC

SOBJECTS=libusb-driver.so libusb-driver-DEBUG.so libusb-driver-trenz.so

all: $(SOBJECTS)

libusb-driver.so: usb-driver.c usb-driver.h
	gcc $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

libusb-driver-trenz.so: usb-driver.c usb-driver.h
	gcc -DTRENZ $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

libusb-driver-DEBUG.so: usb-driver.c usb-driver.h
	gcc -DDEBUG $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

clean:
	rm -f $(SOBJECTS)

.PHONY: clean all
