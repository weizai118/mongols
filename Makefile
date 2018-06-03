
PROJECT=libmongols.so
CPPSRC=$(shell find . -type f | egrep *.cpp$$|sed -e 's/.*indexer.cpp$$//')
CPPOBJ=$(patsubst %.cpp,%.o,$(CPPSRC))
CCSRC=$(shell find . -type f | egrep *.cc$$)
CCOBJ=$(patsubst %.cc,%.o,$(CCSRC))
CXXSRC=$(shell find . -type f | egrep *.cxx$$)
CXXOBJ=$(patsubst %.cxx,%.o,$(CXXSRC))

CSRC=$(shell find . -type f | egrep *.c$$|sed -e 's/.*indexer.c$$//')
COBJ=$(patsubst %.c,%.o,$(CSRC))

OBJ=$(COBJ) $(CXXOBJ) $(CCOBJ) $(CPPOBJ)

CC=gcc
CXX=g++

CFLAGS+=-O3 -std=c11 -Wall -fPIC 
CFLAGS+=-Iinc/mycpp -Iinc/mycpp/lib -Iinc/mycpp/qlibc -Isrc/qlibc/internal
#CFLAGS+=`pkg-config --cflags openssl`
CFLAGS+=-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CXXFLAGS+=-O3 -std=c++11 -Wall -fPIC 
CXXFLAGS+=-Iinc/mycpp -Iinc/mycpp/lib -Iinc/mycpp/qlibc -Isrc/qlibc/internal
#CXXFLAGS+=`pkg-config --cflags openssl` 
CXXFLAGS+=-D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
#LDLIBS+=`pkg-config --libs openssl`  -lpcre -lz -lpthread -ldl -lstdc++
LDFLAGS+=-shared


ifndef INSTALL_DIR
INSTALL_DIR=/usr/local
endif


all:$(PROJECT)

$(PROJECT):$(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS) 

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS)  -c $< -o $@

.cc.o:
	$(CXX) $(CXXFLAGS)  -c $< -o $@
	
.cxx.o:
	$(CXX) $(CXXFLAGS)  -c $< -o $@

clean:
	@for i in $(OBJ);do echo "rm -f" $${i} && rm -f $${i} ;done
	rm -f $(PROJECT)

install:
	test -d $(INSTALL_DIR)/ || mkdir -p $(INSTALL_DIR)/
	install $(PROJECT) $(INSTALL_DIR)/lib
	cp -R inc/mongols $(INSTALL_DIR)/include

