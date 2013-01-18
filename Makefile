CXX = clang++

all: ctest

ctest: ctest.cc $(wildcard calculon*.h)
	$(CXX) -g -o $@ $< $(shell llvm-config-3.2 --cflags --ldflags --libs)
	