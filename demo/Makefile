# Builds the sample Calculon programs. Note that Calculon does not need
# compilation on its own, it's just a bunch of headers!

CXX = g++
#CXX = clang++

LLVM = -I$(shell llvm-config-3.2 --includedir) -lLLVM-3.2
CFLAGS = -O3 -I../include

all: fractal noise filter

clean:
	rm -f fractal noise filter

fractal: fractal.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lboost_program_options
	
noise: noise.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lnoise -lboost_program_options
		
filter: filter.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lboost_program_options
