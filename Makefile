# Calculon © 2017 David Given
# Calculon is a header-only library, so you don't need to build it if
# you really don't want to. This makefile is used for integration
# only.

# Configuration.

CXX = g++
#CXX = clang++

# Assume a generic Unixoid.
BOOST =
NOISE = -lnoise -DNOISEINC=\"libnoise/noise.h\"
LLVM = -I$(shell llvm-config-3.7 --includedir --ldflags) -lLLVM-3.7

CFLAGS = -g -Iinclude $(BOOST)
CALCULON = $(wildcard include/calculon*.h)

all: demo/fractal demo/noise demo/filter

clean:
	rm -f fractal noise filter
	rm -f fractal.o noise.o filter.o

demo/%: demo/%.cc Makefile $(CALCULON)
	$(CXX) $(CFLAGS) -o $@ $< $(LLVM) $(NOISE) -lboost_program_options

TESTS = \
	assigned-return \
	two-returns \
	type-aliases \
	copy \
	vector-splat \
	global-vector-variables \
	global-variables \
	vector-squarebrackets \
	vector-arithmetic \
	dot \
	sum \
	n-vector-types \
	n-vector-accessors \
	arithmetic \
	return-in-function \
	missing-return \
	duplicate-return
	
.PHONY: test
test: demo/filter
	for t in $(TESTS); do \
		echo $$t; \
		(cd tests && ./runtest float $$t); \
		(cd tests && ./runtest double $$t); \
	done

