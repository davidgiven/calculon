#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>

#undef NDEBUG
#include "calculon.h"

int main(int argc, const char* argv[])
{
	typedef double Number;

	Calculon::StandardSymbolTable symbols;

	typedef Number TestFunc();
	std::ifstream code("test.cal");
	Calculon::Program<Number, TestFunc> func(symbols, code, "()");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	func();

	return 0;
}
