ifneq ($(KERNELRELEASE),)
ccflags-y += -O -g
obj-m:= math.o 
math-y:= e_pow.o e_sqrt.o mathcalls.o s_fabs.o e_scalb.o s_rint.o s_finite.o s_scalbn.o s_copysign.o s_isnan.o s_nan.o  s_copysign.o
#浮点运算
EXTRA_CFLAGS:=-mhard-float
else
KERNELBUILD:=/lib/modules/$(shell uname -r)/build

default:

	@echo "BUILD kmod"


	@make -C $(KERNELBUILD) M=$(shell pwd) modules

endif
clean:

	@echo "CLEAN kmod"

	@rm -rf *.o

	@rm -rf .depend .*.cmd *.mod.c .tmp_versions *.symvers .*.d *.markers *.order *.ko
