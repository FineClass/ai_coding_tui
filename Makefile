TOPDIR = $(shell pwd)

CXX = g++

FTXUI = $(TOPDIR)/libftxui

TARG = acpre_tui
OBJS = main.o acpre.o

CXXFLAGS = -Wall -O2 -std=c++17 -I$(FTXUI)/include
# `ftxui-component` must be first in the linking order relative to the other FTXUI libraries
LDFLAGS = -L$(FTXUI)/lib -lftxui-component -lftxui-dom -lftxui-screen

all: $(FTXUI) $(TARG)

$(FTXUI): FTXUI-6.1.9
	@rm -rf FTXUI-6.1.9/build && mkdir FTXUI-6.1.9/build && cd FTXUI-6.1.9/build \
	&& cmake -DCMAKE_INSTALL_PREFIX:PATH=$(FTXUI) .. \
	&& make -j8 && make install

FTXUI-6.1.9: FTXUI-6.1.9.tar.gz
	tar xf FTXUI-6.1.9.tar.gz

$(TARG): $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@

$(OBJS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

clean:
	rm -f *.o $(TARG)
