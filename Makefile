TARGET=pwmservo
obj-m:= $(TARGET).o

$(TARGET).ko: $(TARGET).c
	make -C /usr/src/linux M=`pwd` V=1 modules
clean:
	make -C /usr/src/linux M=`pwd` V=1 clean
