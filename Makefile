# Makefile for japkg
# Set up for cross compiling with ro-make

INCLUDE_DIRS=-I$(GCCSDK_INSTALL_ENV)/include
LIB_DIRS=-L$(GCCSDK_INSTALL_ENV)/lib

LD = g++
CXXFLAGS=-std=c++0x $(INCLUDE_DIRS) -O0 -g3 -Wall -c -fmessage-length=0
LDFLAGS=$(LIB_DIRS) -static -ltbx -lziparch
TARGET=japkg,ff8
ELFTARGET=japkg,e1f

CCSRC = $(wildcard *.cc)
OBJECTS = $(CCSRC:.cc=.o)

$(TARGET):	$(ELFTARGET)
	elf2aif $(ELFTARGET) $(TARGET)

$(ELFTARGET):	$(OBJECTS)
	$(LD) $(LDFLAGS) -o $(ELFTARGET) $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(TARGET) $(ELFTARGET) $(CCSRC:.cc=.d)

-include $(CCSRC:.cc=.d)

