name=vlinky-net

obj-m=${name}.o

${name}-objs= vlinky_pci.o queue.o 



KDIR=/lib/modules/$(shell uname -r)/build
PWD=$(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	insmod ${name}.ko
uninstall:
	-rmmod ${name}
reset:
	make uninstall -C .
	make install -C .
