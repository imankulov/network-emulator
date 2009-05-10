# Set up enviromenent variable PJBASE:
#    PJBASE=/path/to/pjproject/installation
# or run make with this variable set up
#    make PJBASE=... all
MARKDOWN=/usr/share/python-support/python-markdown/markdown.py

include $(PJBASE)/build.mak

CC      = $(APP_CC)
LDFLAGS = $(APP_LDFLAGS)
LDLIBS  = $(APP_LDLIBS) 
CFLAGS  = $(APP_CFLAGS) -g -I.
CPPFLAGS= ${CFLAGS} 



all: emulator
emulator: emulator.o markov_port.o plc_port.o silence_port.o
%.o: %.c %.h
clean:
	rm -f *.o emulator *.html
doc: README.html

%.html: %.md
	$(MARKDOWN) $< > $@

.PHONY: all clean doc
