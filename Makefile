CXX = clang++
LLVM = $(shell llvm-config-3.2 --cflags --ldflags --libs)
#LLVM = $(shell /tmp/llvm/bin/llvm-config --cflags --ldflags --libs)

all: ctest

ctest: ctest.cc Makefile $(wildcard calculon*.h)
	$(CXX) -g -o $@ $< $(LLVM)
	
