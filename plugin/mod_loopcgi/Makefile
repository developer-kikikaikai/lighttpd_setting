include ../lightttpd_setting

TARGET=mod_loopcgi
RMTARGET=$(TARGET).so
SRC=$(shell find . -maxdepth 1 -name "*.c")
CFLAG=$(LIGHTTPD_CFLAGS) -O2 -shared -fPIC

#if I want to check memory leak test, change folliwing option and use valgring
CFLAG+=-O0 -g 
CFLAG+=-DDBGFLAG
INCLUDE=-I $(LIGHTTPD_PATH)/src -I $(LIGHTTPD_PATH)/ -I $(LIGHTTPD_PATH)/m4 -I $(LIGHTTPD_PATH)/scripts/m4
all:
	gcc $(CFLAG) $(INCLUDE) $(OBJ) $(LIBS) -o $(TARGET).so $(SRC)

clean:
	rm -f $(OBJ) $(RMTARGET);
