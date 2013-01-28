CXX = clang++
LLVM = -I$(shell llvm-config-3.2 --includedir) -lLLVM-3.2
#LLVM = $(shell /tmp/llvm/bin/llvm-config --cflags --ldflags --libs)
CFLAGS = -O3

all: ctest fractal noise

clean:
	rm -f ctest fractal noise

ctest: ctest.cc Makefile $(wildcard calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM)
	
fractal: fractal.cc Makefile $(wildcard calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lboost_program_options
	
noise: noise.cc Makefile $(wildcard calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lnoise -lboost_program_options
	
	