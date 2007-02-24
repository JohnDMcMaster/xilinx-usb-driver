CFLAGS=-Wall

xilinx.so: xilinx.c xilinx.h
	gcc $(CFLAGS) $< -o $@ -ldl -lusb -lpthread -shared

clean:
	rm -f xilinx.so
