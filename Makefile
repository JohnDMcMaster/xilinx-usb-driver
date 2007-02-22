CFLAGS=-Wall

xilinx.so: xilinx.c xilinx.h
	gcc $(CFLAGS) $< -o $@ -ldl -lusb -shared

clean:
	rm -f xilinx.so
