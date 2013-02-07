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

template <typename Real>
static bool readnumber(Real& d)
{
	string s;
	if (!(std::cin >> s))
		return false;

	const char* p = s.c_str();
	char* endp;
	d = strtod(p, &endp);
	if (*endp)
	{
		std::cerr << "filter: malformed number in input data";
		exit(1);
	}

	return true;
}

template <typename Settings>
static void process_data(std::istream& codestream, const string& typesignature,
		bool dump)
{
	typedef Calculon::Instance<Settings> Compiler;
	typedef typename Compiler::Real Real;
	typedef typename Compiler::Vector Vector;

	typename Compiler::StandardSymbolTable symbols;

	typedef Real TranslateFunction(Real n);
	typename Compiler::template Program<TranslateFunction> func(symbols, codestream,
			typesignature);
	if (dump)
		func.dump();

	Real d;
	while (readnumber(d))
	{
		std::cout << func(d) << "\n";
	}
}

template <typename Settings>
static void process_data_rows(std::istream& codestream, const string& typesignature,
		bool dump, unsigned vsize)
{
	typedef Calculon::Instance<Settings> Compiler;
	typedef typename Compiler::Real Real;
	typedef typename Compiler::Vector Vector;

	typename Compiler::StandardSymbolTable symbols;

	typedef void TranslateFunction(Real* out, Real* in);
	typename Compiler::template Program<TranslateFunction> func(symbols, codestream,
			typesignature);
	if (dump)
		func.dump();

	Real in[vsize];
	Real out[vsize];

	for (;;)
	{
		for (unsigned i = 0; i < vsize; i++)
			if (!readnumber(in[i]))
			{
				if (i != 0)
					std::cerr << "filter: found partial row, aborting\n";
				return;
			}

		func(out, in);

		for (unsigned i = 0; i < vsize; i++)
			std::cout << out[i] << " ";
		std::cout << "\n";
	}
}

int main(int argc, const char* argv[])
{
	po::options_description options("Allowed options");
	options.add_options()
	    ("help,h",
	    		"produce help message")
	    ("file,f",   po::value<string>(),
	    		"input Calculon script name")
	    ("script,s", po::value<string>(),
	    		"literal Calculon script")
	    ("precision,p", po::value<string>()->default_value("double"),
	    		"specifies whether to use double or float precision")
   		("dump,d",
   				"dump LLVM bitcode after compilation")
   		("vector,v", po::value<unsigned>(),
   				"read each row of values as a vector this big")
   	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, options), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		std::cout << "filter: reads a list of numbers from stdin, processes each one with a\n"
				     "Calculon script, and writes them to stdout.\n"
		          << options <<
		             "\n"
		             "Try: echo 1 | filter --script 'sin(n)'\n";

		exit(1);
	}

	if (!vm.count("file") && !vm.count("script"))
	{
		std::cerr << "filter: you must specify the Calculon script to use!\n"
				     "(try --help)\n";
		exit(1);
	}

	if (vm.count("file") && vm.count("script"))
	{
		std::cerr << "filter: you can't specify *both* a file and a literal script!\n"
			     "(try --help)\n";
		exit(1);
	}

	std::istream* codestream;
	if (vm.count("file"))
	{
		string scriptfilename = vm["file"].as<string>();
		codestream = new std::ifstream(scriptfilename.c_str());
	}
	else
	{
		string script = vm["script"].as<string>();
		codestream = new std::stringstream(script);
	}
	bool dump = (vm.count("dump") > 0);
	string precision = vm["precision"].as<string>();
	unsigned vsize = 0;
	if (vm.count("vector"))
		vsize = vm["vector"].as<unsigned>();

	if ((precision != "float") && (precision != "double"))
	{
		std::cerr << "filter: precision must be 'double' or 'float'\n"
				  << "(try --help)\n";
		exit(1);
	}

	string typesignature;
	if (vsize == 0)
		typesignature = "(n: real): real";
	else
	{
		std::stringstream s;
		s << "(n: vector*" << vsize << "): vector*" << vsize;
		typesignature = s.str();
	}

	if (vsize == 0)
	{
		/* Data is a simple stream of numbers. */
		if (precision == "double")
			process_data<Calculon::RealIsDouble>(*codestream, typesignature, dump);
		else
			process_data<Calculon::RealIsFloat>(*codestream, typesignature, dump);
	}
	else
	{
		/* Data is a stream of rows. */
		if (precision == "double")
			process_data_rows<Calculon::RealIsDouble>(*codestream,
					typesignature, dump, vsize);
		else
			process_data_rows<Calculon::RealIsFloat>(*codestream,
					typesignature, dump, vsize);
	}

	return 0;
}
