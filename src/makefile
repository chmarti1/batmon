test: test.c autocom.o
	gcc -o test autocom.o test.c -lm -llabjackusb

bmcmd: bmcmd.c autocom.o -lm -llabjackusb

autocom.o: autocom.c autocom.h
	gcc -c autocom.c -o autocom.o
