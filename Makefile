CC = clang
CFLAGS = -Wall -Wextra -g -fPIC
PREFIX = /usr/local
LDLIBS = -luring

liburing:
	git submodule update --init -- liburing
	cd liburing && ./configure --prefix=$(PREFIX)
	$(MAKE) -C liburing
	$(MAKE) -C liburing liburing.pc
	$(MAKE) -C liburing install
	ldconfig


liboml.so: src/liboml.c src/liboml.h
	$(CC) $(CFLAGS) -DOML_QUIET -shared src/liboml.c -o $@ $(LDLIBS)

install: liboml.so
	install -d $(PREFIX)/include
	install -d $(PREFIX)/lib
	install -m 644 src/liboml.h $(PREFIX)/include/
	install -m 755 liboml.so $(PREFIX)/lib/
	ldconfig

clean:
	rm -f liboml.so

.PHONY: liburing install clean
.DEFAULT_GOAL := liboml.so