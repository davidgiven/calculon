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

	typedef void TestFunc(Vector* v, Vector* v1, Vector* v2);
	std::ifstream code("test.cal");
	Compiler::Program<TestFunc> func(symbols, code, "(p:vector)");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	Vector v;
	Vector v1 = {1, 2, 3};
	Vector v2 = {4, 5, 6};
	func(&v, &v1, &v2);
	std::cout << "v={" << v.x << ", " << v.y << ", " << v.z << "};\n";

	return 0;
}
