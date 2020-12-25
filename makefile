

obj-m += daq.o
daq-objs := daq.o usb_4711a.o

include ./Makefile.modinc

LDFLAGS += -lbiodaq -lrdkafka -lz -lpthread -lrt -fgnu89-inline


clean:
	rm *.o *.so *.sym *.tmp *.ver
