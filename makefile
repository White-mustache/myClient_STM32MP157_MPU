#
# Makefile for Client
#
all:
	 arm-buildroot-linux-gnueabihf-gcc -o client myClient_MPU.c -lpthread

clean:
	rm client
