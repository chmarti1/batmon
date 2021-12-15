test: test.c autocom.o
	gcc -Wall -o test autocom.o test.c -lm -llabjackusb

bmcmd: bmcmd.c autocom.o 
	gcc -Wall -o bmcmd autocom.o bmcmd.c -lm -llabjackusb

autocom.o: autocom.c autocom.h
	gcc -Wall -c autocom.c -o autocom.o
