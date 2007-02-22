xilinx.so: xilinx.c xilinx.h
	gcc $< -o $@ -ldl -shared

clean:
	rm -f xilinx.so
