CXX = clang++
LLVM = $(shell llvm-config-3.2 --cflags --ldflags --libs)
#LLVM = $(shell /tmp/llvm/bin/llvm-config --cflags --ldflags --libs)

all: ctest fractal

ctest: ctest.cc Makefile $(wildcard calculon*.h)
	$(CXX) -g -o $@ $< $(LLVM)
	
fractal: fractal.cc Makefile $(wildcard calculon*.h)
	$(CXX) -g -o $@ $< $(LLVM) -lboost_program_options
	
	