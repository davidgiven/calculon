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

Compiler::StandardSymbolTable symbols;

int main(int argc, const char* argv[])
{
	Real minr = -2.0;
	Real mini = -1.0;
	Real maxr = 1.0;
	Real maxi = 1.0;
	unsigned width = 1024;
	unsigned height = 1024;
	string scriptfilename = "fractal.cal";
	string outputfilename = "fractal.pgm";

	po::options_description options("Allowed options");
	options.add_options()
	    ("help,h",
	    		"produce help message")
	    ("minr",     po::value(&minr),
	    		"minimum real part")
	    ("mini",     po::value(&mini),
	    		"minimum imaginary part")
	    ("maxr",     po::value(&maxr),
	    		"maximum real part")
	    ("maxi",     po::value(&maxi),
	    		"maximum imaginary part")
	    ("width,x",  po::value(&width),
	    		"width of output image")
	    ("height,y", po::value(&height),
	    		"height of output image")
	    ("file,f",   po::value(&scriptfilename),
	    		"input Calculon script name")
	    ("dump,d",
	    		"dump LLVM bitcode after compilation")
	    ("output,o", po::value(&outputfilename),
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

	bool dump = (vm.count("dump") > 0);

	/* Load the Calculon function to generate the pixels. */

	typedef Real FractalFunction(Real r, Real i, Real* intensity);
	std::ifstream code(scriptfilename.c_str());
	Compiler::Program<FractalFunction> func(symbols, code, "(r:real, i:real): (intensity:real)");
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
			Real intensity;

			func(r, i, &intensity);
			outputfile << (int)(intensity * 65535.0) << "\n";
		}
	}

	return 0;
}
