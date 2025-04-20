C_FILES = $(shell find . -name "*.c")
S_OBJ=$(patsubst %c, %o, $(C_FILES))
CC = gcc
CFLAGS = -static -lrabbitmq -L/usr/local/lib/arm-linux-gnueabihf -lrabbitmq

all: src/inter-temperature

src/inter-temperature: $(S_OBJ)
	$(CC)  $^ $(CFLAGS) -o $@ 

%.o : %.c %.h
	$(CC) -c  $^ $(CFLAGS) 
	mv *.o ./src 

install: src/inter-temperature
	install -D src/inter-temperature \
		$(DESTDIR)$(prefix)/bin/inter-temperature

clean:
	-rm -f src/*.o all inter-temperature
	rm -rf usr


distclean: clean

uninstall:
	-rm -f $(DESTDIR)$(prefix)/bin/inter-temperature

.PHONY: all install clean distclean uninstall test create_package

