obj-m += arc.o test.o
kern_path=/lib/modules/`uname -r`/build
all:
	make -C $(kern_path) M=$(PWD) modules
clean:
	make -C $(kern_path) M=$(PWD) clean
