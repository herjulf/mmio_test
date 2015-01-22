CFLAGS?=-Wall
#LDFLAGS?=-static
LDLIBS?=-lpci
mmio_test:	mmio_test.o

install:	mmio_test
	strip mmio_test
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	cp -p mmio_test $(DESTDIR)/$(PREFIX)/bin
clean:
	rm -f *.o mmio_test

