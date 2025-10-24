Pintool compile command:
```
make
```

To use the trace generator, you need to load the program via the Intel Pin.

Example command:
./pin/pin -t ./obj-intel64/my_pintool.so -o my_dir -- ./my_program

Options:
-f: Force instrumentation even when no blocks are defined.
-o: Output directory.