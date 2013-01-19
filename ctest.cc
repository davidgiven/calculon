#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>

#undef NDEBUG
#include "calculon.h"

int main(int argc, const char* argv[])
{
	typedef double Number;
	struct Vector { Number x, y, z; };

	Calculon::StandardSymbolTable symbols;

	typedef Vector TestFunc();
	std::ifstream code("test.cal");
	Calculon::Program<Number, TestFunc> func(symbols, code, "():vector");
	std::cout << "size of Program object: " << sizeof(func) << "\n";
	func.dump();

	Vector v = func();
	std::cout << "v={" << v.x << ", " << v.y << ", " << v.z << "};\n";

	return 0;
}
