#ifndef CALCULON_H
#define CALCULON_H

#include <stdexcept>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <cassert>
#include <cctype>
#include <memory>
#include "llvm/DerivedTypes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/IRBuilder.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/DataLayout.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/TargetSelect.h"

namespace Calculon
{
	using std::string;
	using std::vector;
	using std::map;
	using std::pair;
	using std::set;
	using std::auto_ptr;

	class CompilationException : public std::invalid_argument
	{
	public:
		CompilationException(const string& what):
			std::invalid_argument(what)
		{
		}
	};


	class SymbolTable
	{
		typedef void (*FuncPtr)();
		typedef std::map<string, FuncPtr> Symbols;

		SymbolTable* _next;
		Symbols  _symbols;

	public:
		SymbolTable(SymbolTable& next):
			_next(&next)
		{
		}

		SymbolTable():
			_next(NULL)
		{
		}

		template <typename T>
		void add(const string& name, const string& signature, T function)
		{
			add(name, signature, (FuncPtr) function);
		}

		FuncPtr get(const string& name, const string& signature)
		{
			Symbols::const_iterator i = _symbols.find(name + " " + signature);
			if (i == _symbols.end())
			{
				if (_next)
					return _next->get(name, signature);
				return NULL;
			}

			return i->second;
		}

	private:
		void add(const string& name, const string& signature, FuncPtr function)
		{
			_symbols[name + " " + signature] = function;
		}
	};

	class StandardSymbolTable : public SymbolTable
	{
	public:
		StandardSymbolTable()
		{
			add("sin", "D>D", sin);
			add("sin", "F>F", sinf);
			add("asin", "D>D", asin);
			add("asin", "F>F", asinf);
			add("cos", "D>D", cos);
			add("cos", "F>F", cosf);
			add("acos", "D>D", acos);
			add("acos", "F>F", acosf);
			add("tan", "D>D", tan);
			add("tan", "F>F", tanf);
			add("atan", "D>D", atan);
			add("atan", "F>F", atanf);
			add("atan2", "DD>D", atan2);
			add("atan2", "FF>F", atan2f);
			add("sqrt", "D>D", sqrt);
			add("sqrt", "F>F", sqrtf);
		}
	};

	#include "calculon_lexer.h"
	#include "calculon_compiler.h"

	template <class Real, typename FuncType> class Program
	{
	private:
		llvm::LLVMContext _context;
		const SymbolTable& _symbols;
		llvm::Module* _module;
		llvm::ExecutionEngine* _engine;
		llvm::Function* _function;
		llvm::BasicBlock* _toplevel;
		FuncType* _funcptr;

		typedef Lexer<Real> L;

	public:
		Program(const SymbolTable& symbols, const string& code, const string& signature):
				_symbols(symbols),
				_funcptr(NULL)
		{
			std::istringstream stream(code);
			init(stream, signature);
		}

		Program(const SymbolTable& symbols, std::istream& code, const string& signature):
				_symbols(symbols),
				_funcptr(NULL)
		{
			init(code, signature);
		}

		~Program()
		{
		}

		operator FuncType* () const
		{
			return _funcptr;
		}

		void dump()
		{
			_module->dump();
		}

	private:

	private:
		void init(std::istream& codestream, const string& signature)
		{
			_module = new llvm::Module("Calculon Function", _context);

			llvm::InitializeNativeTarget();

			string s;
			_engine = llvm::EngineBuilder(_module)
				.setErrorStr(&s)
				.create();
			if (!_engine)
				throw CompilationException(s);

#if 0
			/* Parse the main function signature. */

			vector< pair<string, char> > arguments;
			char returntype;

			{
				std::istringstream signaturestream(signature);
				L lexer(signaturestream);

				parse_type_signature(lexer, arguments, returntype);
				expect_eof(lexer);
			}
#endif

			/* Create the toplevel function from this signature. */

			llvm::FunctionType* ft = llvm::FunctionType::get(
					llvm::Type::getVoidTy(_context),
					false);

			_function = llvm::Function::Create(ft,
					llvm::Function::ExternalLinkage,
					"Calculon Function", _module);

			_toplevel = llvm::BasicBlock::Create(_context, "entry", _function);

//			_builder.SetInsertPoint(_toplevel);

			Compiler<Real> compiler(_context);

			std::istringstream signaturestream(signature);
			compiler.parse(signaturestream, codestream);
//			compiler.builder.SetInsertPoint(_toplevel);
//			compiler.builder.CreateRet(c.codegen(compiler));

			generate_machine_code();
		}

	private:
		void expect_operator(L& lexer, const string& s)
		{
			if ((lexer.token() != L::OPERATOR) || (lexer.id() != s))
				lexer.error(string("expected '"+s+"'"));
			lexer.next();
		}

		void expect_eof(L& lexer)
		{
			if (lexer.token() != L::EOF)
				lexer.error("expected EOF");
		}

		void parse_type_signature(L& lexer, vector<pair<string, char> >& arguments,
				char& returntype)
		{
			expect_operator(lexer, "(");
			expect_operator(lexer, ")");
		}

		void generate_machine_code()
		{
			llvm::FunctionPassManager fpm(_module);

			// Set up the optimizer pipeline.  Start with registering info about how the
			// target lays out data structures.
			fpm.add(new llvm::DataLayout(*_engine->getDataLayout()));
			// Provide basic AliasAnalysis support for GVN.
			fpm.add(llvm::createBasicAliasAnalysisPass());
			// Do simple "peephole" optimizations and bit-twiddling optzns.
			fpm.add(llvm::createInstructionCombiningPass());
			// Reassociate expressions.
			fpm.add(llvm::createReassociatePass());
			// Eliminate Common SubExpressions.
			fpm.add(llvm::createGVNPass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			fpm.add(llvm::createCFGSimplificationPass());

			fpm.doInitialization();
			llvm::verifyFunction(*_function);
			fpm.run(*_function);

			_funcptr = (FuncType*) _engine->getPointerToFunction(_function);
		}
	};
}

#endif
