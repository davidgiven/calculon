#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>

#undef NDEBUG
#include "calculon.h"

int main(int argc, const char* argv[])
{
	typedef Calculon::Instance<Calculon::RealIsFloat> Compiler;
	typedef Compiler::Real Real;
	typedef Compiler::Vector Vector;

	Compiler::StandardSymbolTable symbols;

	typedef Real TestFunc(Real x, Real y, Real z);
	std::ifstream code("test.cal");
	Compiler::Program<TestFunc> func(symbols, code, "(x,y,z)");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	Real result = func(1, 2, 3);
	std::cout << "result=" << result << "\n";

	return 0;
}
