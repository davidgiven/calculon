# Builds the sample Calculon programs. Note that Calculon does not need
# compilation on its own, it's just a bunch of headers!

CXX = g++
#CXX = clang++

BOOST =
NOISE = -lnoise -DNOISEINC=\"libnoise/noise.h\"
LLVM = -I$(shell llvm-config-3.3 --includedir) -lLLVM-3.3
#LLVM = $(shell /tmp/llvm/bin/llvm-config --cflags --ldflags --libs)
CFLAGS = -g -I../include $(BOOST)

all: fractal noise filter

osx:
	$(MAKE) \
		BOOST="-I$(shell brew --prefix)/include \
			-L$(shell brew --prefix)/lib" \
		NOISE="-I$(shell brew --prefix)/Cellar/libnoise/1.0.0/include \
			-L$(shell brew --prefix)/Cellar/libnoise/1.0.0/lib \
			-lnoise -DNOISEINC=\\\"noise/noise.h\\\"" \
		LLVM="-I$(shell llvm-config-3.3 --includedir) \
			-L$(shell llvm-config-3.3 --libdir) \
			-lLLVM-3.3"

clean:
	rm -f fractal noise filter

fractal: fractal.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) -lboost_program_options
	
noise: noise.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) $(NOISE) -lboost_program_options
		
filter: filter.cc Makefile $(wildcard ../include/calculon*.h)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) $(NOISE) -lboost_program_options
