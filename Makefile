PREFIX ?= $(HOME)/.local
CTOS_ROOT ?= /opt/ctos
CC ?= gcc
CFLAGS ?= -shared -fPIC -O2 -Wall -Wextra
LDLIBS ?= -ldl -lxcb -lpthread

.PHONY: all install patch-lockd clean

all: libctos-x11-lock-grab.so

libctos-x11-lock-grab.so: src/grab.c
	$(CC) $(CFLAGS) -o $@ src/grab.c $(LDLIBS)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(CTOS_ROOT)
	install -m 755 bin/ctos-lock $(DESTDIR)$(PREFIX)/bin/ctos-lock
	install -m 755 libctos-x11-lock-grab.so $(DESTDIR)$(PREFIX)/lib/libctos-x11-lock-grab.so
	install -m 644 qml/x11lock.qml $(DESTDIR)$(CTOS_ROOT)/x11lock.qml

patch-lockd:
	patch -d $(CTOS_ROOT) -p1 -N < patches/lockd-user.patch

clean:
	rm -f libctos-x11-lock-grab.so
