ifneq ($(KERNELRELEASE),)
ccflags-y += -O -g
obj-m:= sch_rem.o
#浮点运算
EXTRA_CFLAGS:=-mhard-float
EXTRA_CFLAGS+=-I$(INCDIR1)\
			  -I$(INCDIR2)
KBUILD_EXTRA_SYMBOLS:=$(EXTSYM)
else
KERNELBUILD:=/lib/modules/$(shell uname -r)/build

default:
	@echo "BUILD kmod"
	@make -C $(KERNELBUILD) M=$(shell pwd) INCDIR1=$(shell pwd)/../include/ INCDIR2=$(shell pwd)/../math/ EXTSYM=$(shell pwd)/../math/Module.symvers modules 

endif
clean:
	@echo "CLEAN kmod"
	@rm -rf *.o
	@rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers *.order *.ko
