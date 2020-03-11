KVERS = $(shell uname -r)
obj-m += cdd.o
obj-m += multi_cdd.o

modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
