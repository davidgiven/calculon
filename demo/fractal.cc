/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <boost/program_options.hpp>

#include "calculon.h"

using std::string;
namespace po = boost::program_options;

typedef Calculon::Instance<Calculon::RealIsDouble> Compiler;
typedef Compiler::Real Real;
typedef Compiler::Vector Vector;

Compiler::StandardSymbolTable symbols;

int main(int argc, const char* argv[])
{
	po::options_description options("Allowed options");
	options.add_options()
	    ("help,h",
	    		"produce help message")
	    ("minr",     po::value<Real>()->default_value(-2.0),
	    		"minimum real part")
	    ("mini",     po::value<Real>()->default_value(-1.0),
	    		"minimum imaginary part")
	    ("maxr",     po::value<Real>()->default_value(1.0),
	    		"maximum real part")
	    ("maxi",     po::value<Real>()->default_value(1.0),
	    		"maximum imaginary part")
	    ("width,x",  po::value<int>()->default_value(1024),
	    		"width of output image")
	    ("height,y", po::value<int>()->default_value(1024),
	    		"height of output image")
	    ("file,f",   po::value<string>()->default_value("fractal.cal"),
	    		"input Calculon script name")
	    ("dump,d",
	    		"dump LLVM bitcode after compilation")
	    ("output,o", po::value<string>()->default_value("fractal.pgm"),
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

	Real minr = vm["minr"].as<Real>();
	Real mini = vm["mini"].as<Real>();
	Real maxr = vm["maxr"].as<Real>();
	Real maxi = vm["maxi"].as<Real>();
	int width = vm["width"].as<int>();
	int height = vm["height"].as<int>();
	string scriptfilename = vm["file"].as<string>();
	string outputfilename = vm["output"].as<string>();
	bool dump = (vm.count("dump") > 0);

	/* Load the Calculon function to generate the pixels. */

	typedef Real FractalFunction(Real r, Real i);
	std::ifstream code(scriptfilename.c_str());
	Compiler::Program<FractalFunction> func(symbols, code, "(r,i)");
	if (dump)
		func.dump();

	/* Open the output file. */

	std::ofstream outputfile(outputfilename.c_str());
	outputfile << "P2\n" << width << "\n" << height << "\n" << "65535\n";

	Real rw = maxr - minr;
	Real rh = maxi - mini;

	for (Real y = 0; y < height; y++)
	{
		for (Real x = 0; x < width; x++)
		{
			Real r = minr + rw*(x/width);
			Real i = mini + rh*(y/height);

			Real result = func(r, i);
			outputfile << (int)(result * 65535.0) << "\n";
		}
	}

	return 0;
}
