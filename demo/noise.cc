/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <boost/program_options.hpp>
#include "libnoise/noise.h"

#include "calculon.h"

using std::string;
namespace po = boost::program_options;

typedef Calculon::Instance<Calculon::RealIsFloat> Compiler;
typedef Compiler::Real Real;
typedef Compiler::Vector Vector;

Compiler::StandardSymbolTable symbols;

extern "C"
double perlin(Vector* v)
{
	static noise::module::Perlin generator;
	return generator.GetValue(v->x, v->y, v->z);
}

int main(int argc, const char* argv[])
{
	po::options_description options("Allowed options");
	options.add_options()
	    ("help,h",
	    		"produce help message")
	    ("width,x",  po::value<int>()->default_value(1024),
	    		"width of output image")
	    ("height,y", po::value<int>()->default_value(1024),
	    		"height of output image")
	    ("file,f",   po::value<string>()->default_value("noise.cal"),
	    		"input Calculon script name")
		("dump,d",
				"dump LLVM bitcode after compilation")
	    ("output,o", po::value<string>()->default_value("noise.ppm"),
	    		"output filename")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, options), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << options << "\n";
		exit(1);
	}

	int width = vm["width"].as<int>();
	int height = vm["height"].as<int>();
	string scriptfilename = vm["file"].as<string>();
	string outputfilename = vm["output"].as<string>();
	bool dump = (vm.count("dump") > 0);

	/* Register the noise function. */

	symbols.add("perlin", "D=V", perlin);

	/* Load the Calculon function to generate the pixels. */

	typedef void FractalFunction(Vector* result, Real r, Real i);
	std::ifstream code(scriptfilename.c_str());
	Compiler::Program<FractalFunction> func(symbols, code, "(x,y):vector");
	if (dump)
		func.dump();

	/* Open the output file. */

	std::ofstream outputfile(outputfilename.c_str());
	outputfile << "P3\n" << width << "\n" << height << "\n" << "65535\n";

	for (Real y = 0; y < height; y++)
	{
		for (Real x = 0; x < width; x++)
		{
			Real xx = -1 + x * (2/(Real)width);
			Real yy = -1 + y * (2/(Real)height);

			Vector result;
			func(&result, xx, yy);

			outputfile << (int)(result.x * 65535.0) << " "
					   << (int)(result.y * 65535.0) << " "
					   << (int)(result.z * 65535.0) << "\n";
		}
	}

	return 0;
}
