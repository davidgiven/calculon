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
using std::vector;
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
		bool dump, unsigned ivsize, unsigned ovsize)
{
	typedef Calculon::Instance<Settings> Compiler;
	typedef typename Compiler::Real Real;
	typedef typename Compiler::template Vector<4> Vector4;

	typename Compiler::StandardSymbolTable symbols;

	typedef void TranslateFunction(Real* out, Real* in);
	typename Compiler::template Program<TranslateFunction> func(symbols, codestream,
			typesignature);
	if (dump)
		func.dump();

	/* Important! Vectors must be correctly aligned. This is harder than it
	 * looks. We allocate an vector of Compiler::Vector<4> to force the
	 * alignment to be correct, and then take the address of the first
	 * element to get raw Reals.
	 */

	vector<Vector4> istorage((ivsize+3) / 4);
	Real* in = &istorage[0].m[0];

	vector<Vector4> ostorage((ovsize+3) / 4);
	Real* out = &ostorage[0].m[0];

	for (;;)
	{
		for (unsigned i = 0; i < ivsize; i++)
			if (!readnumber(in[i]))
			{
				if (i != 0)
					std::cerr << "filter: found partial row, aborting\n";
				return;
			}

		func(out, in);

		for (unsigned i = 0; i < ovsize; i++)
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
   		("ivector,i", po::value<unsigned>(),
   				"read each row of values as a vector this big")
   		("ovector,o", po::value<unsigned>(),
   				"return the result as a vector this big")
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
					 "If you use --ivector, you must also use --ovector (but you're allowed a\n"
					 "vector with one element).\n"
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

	unsigned ivsize = 0;
	if (vm.count("ivector"))
		ivsize = vm["ivector"].as<unsigned>();

	unsigned ovsize = 0;
	if (vm.count("ovector"))
		ovsize = vm["ovector"].as<unsigned>();

	if ((ivsize != 0) != (ovsize != 0))
	{
		std::cerr << "filter: if the input is a vector, the output must be too (and vice versa)\n"
		          << "(try --help)\n";
		exit(1);
	}

	if ((precision != "float") && (precision != "double"))
	{
		std::cerr << "filter: precision must be 'double' or 'float'\n"
				  << "(try --help)\n";
		exit(1);
	}

	string typesignature;
	if (ivsize == 0)
		typesignature = "(n: real): real";
	else
	{
		std::stringstream s;
		s << "(n: vector*" << ivsize << "): vector*" << ovsize;
		typesignature = s.str();
	}

	if (ivsize == 0)
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
					typesignature, dump, ivsize, ovsize);
		else
			process_data_rows<Calculon::RealIsFloat>(*codestream,
					typesignature, dump, ivsize, ovsize);
	}

	return 0;
}
