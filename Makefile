CFLAGS=-Wall

libusb-driver.so: usb-driver.c usb-driver.h
	gcc $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

clean:
	rm -f libusb-driver.so

.PHONY: clean
