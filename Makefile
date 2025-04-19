C_FILES = $(shell find . -name "*.c")
S_OBJ=$(patsubst %c, %o, $(C_FILES))
CC = gcc
CFLAGS = -static -lrabbitmq -L/usr/local/lib/arm-linux-gnueabihf -lrabbitmq

all: temperature

temperature: $(S_OBJ)
	$(CC)  $^ $(CFLAGS) -o $@ 

%.o : %.c %.h
	$(CC) -c  $^ $(CFLAGS) 
	mv *.o ./src 

install: src/temperature
	install -D src/temperature \
		$(DESTDIR)$(prefix)/bin/usage

clean:
	-rm -f src/*.o all temperature

distclean: clean

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/temperature

.PHONY: all install clean distclean uninstall test
