CC = clang
CFLAGS = -Wall -Wextra -g -fPIC
PREFIX = /usr/local

liboml.so: src/liboml.c src/liboml.h
	$(CC) $(CFLAGS) -shared src/liboml.c -o $@

install: liboml.so
	install -d $(PREFIX)/include
	install -d $(PREFIX)/lib
	install -m 644 src/liboml.h $(PREFIX)/include/
	install -m 755 liboml.so $(PREFIX)/lib/
	ldconfig

clean:

.PHONY: install clean

	rm -f liboml.so