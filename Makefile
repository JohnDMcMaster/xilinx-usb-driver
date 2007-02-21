xilinx.so: xilinx.c
	gcc $< -o $@ -ldl -shared

clean:
	rm -f xilinx.so
