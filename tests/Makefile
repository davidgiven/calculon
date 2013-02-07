# Builds the sample Calculon programs. Note that Calculon does not need
# compilation on its own, it's just a bunch of headers!

FILTER = ../demo/filter

TESTS = \
	simple \
	add \
	sub \
	mul \
	div \
	n-vector-types
	
all: $(FILTER) tests

$(FILTER): ../demo/filter.cc $(wildcard ../include/calculon*.h)
	make -C ../demo filter
	
.PHONY: tests
tests:
	for t in $(TESTS); do \
		echo $$t; \
		$(FILTER) -p double -f $$t.cal < testdata > $$t.double.dirty; \
		(diff -uN $$t.double.dirty $$t.clean && rm $$t.double.dirty) || echo "TEST FAILED"; \
		$(FILTER) -p float -f $$t.cal < testdata > $$t.float.dirty; \
		(diff -uN $$t.float.dirty $$t.clean && rm $$t.float.dirty) || echo "TEST FAILED"; \
	done