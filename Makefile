# Calculon © 2014 David Given
# Calculon is a header-only library, so you don't need to build it if
# you really don't want to. This makefile is used for integration
# only.

# Configuration.

CXX = g++
#CXX = clang++

UNAME=$(shell uname)
ifeq ($(UNAME), Darwin)
	# Detect OSX.
	BOOST = \
		-I$(shell brew --prefix)/include \
		-L$(shell brew --prefix)/lib
	NOISE = \
		-I$(shell brew --prefix)/Cellar/libnoise/1.0.0/include \
		-L$(shell brew --prefix)/Cellar/libnoise/1.0.0/lib \
		-DNOISEINC=\"noise/noise.h\" \
		-lnoise
	LLVM = \
		-I$(shell llvm-config-3.3 --includedir) \
		-L$(shell llvm-config-3.3 --libdir) \
		-lLLVM-3.3
else
	# Assume a generic Unixoid.
	BOOST =
	NOISE = -lnoise -DNOISEINC=\"libnoise/noise.h\"
	LLVM = -I$(shell llvm-config-3.3 --includedir) -lLLVM-3.3
endif

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

