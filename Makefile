TARGET := boogie.ko
VERBOSITY = 0
REL_VERSION = "0.2"
REL_DATE = "2012-10-31"
EXTRA_CFLAGS += -Wformat=2

all: ${TARGET}

boogie.ko: boogie.c
	make -C /lib/modules/$(shell uname -r)/build M=${shell pwd} V=$(VERBOSITY) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=${shell pwd} V=$(VERBOSITY) clean

clean-files := *.o *.ko *.mod.[co] *~ version.h

obj-m := boogie.o

install: $(TARGET)
	install -m 644 $(TARGET) /lib/modules/$(shell uname -r)/kernel/drivers/input/tablet
	depmod -a
