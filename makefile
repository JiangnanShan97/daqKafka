

obj-m += daq.o
daq-objs := daq.o usb_4711a.o

include ./Makefile.modinc

LDFLAGS += -lbiodaq -lrdkafka -lz -lpthread -lrt -fgnu89-inline


clean:
	rm *.o *.so *.sym *.tmp *.ver \
	acc_x_10000hz.txt acc_y_10000hz.txt acc_z_10000hz.txt \
	force_x_10000hz.txt force_y_10000hz.txt force_z_10000hz.txt \
	def_x_10000hz.txt def_y_10000hz.txt
