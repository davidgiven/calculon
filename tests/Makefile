# Builds the sample Calculon programs. Note that Calculon does not need
# compilation on its own, it's just a bunch of headers!

FILTER = ../demo/filter

TESTS = \
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
	arithmetic
	
all: $(FILTER) tests

$(FILTER): ../demo/filter.cc $(wildcard ../include/calculon*.h)
	make -C ../demo filter
	
.PHONY: tests
tests:
	for t in $(TESTS); do \
		echo $$t; \
		./runtest float $$t; \
		./runtest double $$t; \
	done
