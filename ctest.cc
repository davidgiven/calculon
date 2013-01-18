#include <stdlib.h>
#include <iostream>
#include <math.h>

#undef NDEBUG
#include "calculon.h"

const char* code = "1";

int main(int argc, const char* argv[])
{
	typedef double Number;

	Calculon::StandardSymbolTable symbols;

	typedef Number TestFunc();
	Calculon::Program<Number, TestFunc> func(symbols, code, "()");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	func();

	return 0;
}
