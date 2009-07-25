# Set up enviromenent variable PJBASE:
#    PJBASE=/path/to/pjproject/installation
# or run make with this variable set up
#    make PJBASE=... all
MARKDOWN=/usr/share/python-support/python-markdown/markdown.py
PREFIX ?= /usr
DB2MAN=/usr/bin/docbook2x-man

include $(PJBASE)/build.mak

CC      = $(APP_CC)
LDFLAGS = $(APP_LDFLAGS)
LDLIBS  = $(APP_LDLIBS) 
CFLAGS  = $(APP_CFLAGS) -g -I.
CPPFLAGS= ${CFLAGS} 



all: emulator
install: all man
	install -m 0755 -t $(PREFIX)/bin ./emulator
	gzip -c ./man/emulator.1 > ./man/emulator.1.gz
	install -m 0644 -t $(PREFIX)/share/man/man1 ./man/emulator.1.gz
emulator: emulator.o markov_port.o plc_port.o silence_port.o leaky_bucket_port.o
%.o: %.c %.h
clean:
	rm -f *.o emulator *.html man/emulator.1 man/emulator.1.gz man/emulator.1.pdf man/emulator.1.txt
doc: man README.html man/emulator.1.pdf man/emulator.1.txt
man: man/emulator.1

# documentation from markdown
%.html: %.md
	$(MARKDOWN) $< > $@

# pdf doc
%.pdf: %.xml
	cd man && docbook2pdf  $(notdir $<)

# text doc
%.txt: %.xml
	cd man && docbook2txt  $(notdir $<)

# man pages
%.1: %.1.xml
	cd man && $(DB2MAN) $(notdir $<)

.PHONY: all clean doc man install
