#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>

#undef NDEBUG
#include "calculon.h"

int main(int argc, const char* argv[])
{
	typedef float Number;
	typedef Calculon::Type<Number>::Vector Vector;

	Calculon::StandardSymbolTable symbols;

	typedef void TestFunc(Vector* v, Vector* v1, Vector* v2);
	std::ifstream code("test.cal");
	Calculon::Program<Number, TestFunc> func(symbols, code, "(v1:vector,v2:vector):vector");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	Vector v;
	Vector v1 = {1, 2, 3};
	Vector v2 = {4, 5, 6};
	func(&v, &v1, &v2);
	std::cout << "v={" << v.x << ", " << v.y << ", " << v.z << "};\n";

	return 0;
}
