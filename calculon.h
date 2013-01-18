#ifndef CALCULON_H
#define CALCULON_H

#include <stdexcept>
#include <string>
#include <map>
#include <sstream>
#include <cassert>
#include <cctype>
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
	using std::auto_ptr;
	using std::pair;

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

	#include "lexer.h"

	template <class Real, typename FuncType> class Function
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
		Function(const SymbolTable& symbols, const string& code, const string& signature):
				_symbols(symbols),
				_funcptr(NULL)
		{
			std::istringstream stream(code);
			init(stream, signature);
		}

		Function(const SymbolTable& symbols, std::istream& code, const string& signature):
				_symbols(symbols),
				_funcptr(NULL)
		{
			init(code, signature);
		}

		~Function()
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
		class Compiler
		{
		public:
			Function& function;
			llvm::LLVMContext& context;
			llvm::IRBuilder<> builder;
			llvm::Type* realType;
			llvm::Type* vectorType;
			llvm::Type* structType;

		public:
			Compiler(Function& function):
				function(function),
				context(function._context),
				builder(context)
			{
				realType = createRealType((Real) 0.0);
				vectorType = llvm::VectorType::get(realType, 4);
				structType = llvm::StructType::get(
						realType, realType, realType, NULL);
			}

			class SaveState
			{
				Compiler& _compiler;
				llvm::BasicBlock* _bb;
				llvm::BasicBlock::iterator _i;

			public:
				SaveState(Compiler& compiler):
					_compiler(compiler),
					_bb(compiler.builder.GetInsertBlock()),
					_i(compiler.builder.GetInsertPoint())
				{
				}

				~SaveState()
				{
					_compiler.builder.SetInsertPoint(_bb, _i);
				}
			};

		private:
			llvm::Type* createRealType(double)
			{
				return llvm::Type::getDoubleTy(context);
			}

			llvm::Type* createRealType(float)
			{
				return llvm::Type::getFloatTy(context);
			}
		};

		class ASTNode
		{
			virtual llvm::Value* codegen(Compiler& compiler) = 0;
		};

		class ASTConstant : public ASTNode
		{
			Real _value;

		public:
			ASTConstant(Real value):
				_value(value)
			{
			}

			llvm::Value* codegen(Compiler& compiler)
			{
				return llvm::ConstantFP::get(compiler.context, llvm::APFloat(_value));
			}
		};

		class ASTVector : public ASTNode
		{
			auto_ptr<ASTNode> _x;
			auto_ptr<ASTNode> _y;
			auto_ptr<ASTNode> _z;

		public:
			ASTVector(ASTNode* x, ASTNode* y, ASTNode* z):
				_x(x), _y(y), _z(z)
			{
			}

			llvm::Value* codegen(Compiler& compiler)
			{

			}
		};

		class ASTFunction : public ASTNode
		{
			auto_ptr<ASTNode> _definition;
			auto_ptr<ASTNode> _body;
			llvm::Function* _function;

		public:
			ASTFunction(llvm::Function* function, ASTNode* definition,
					ASTNode* body):
				_function(function),
				_definition(definition),
				_body(body)
			{
			}

			llvm::Value* codegen(Compiler& compiler)
			{
				{
					typename Compiler::SaveState state;

					llvm::BasicBlock* toplevel = llvm::BasicBlock::Create(
							compiler, "", _function);
					compiler.SetInsertPoint(toplevel);

					llvm::Value* v = _body->codegen(compiler);
					compiler.builder.CreateRet(v);
				}

				return _body->codegen(compiler);
			}
		};

	private:
		void init(std::istream& code, const string& signature)
		{
			_module = new llvm::Module("Calculon Function", _context);

			llvm::InitializeNativeTarget();

			string s;
			_engine = llvm::EngineBuilder(_module)
				.setErrorStr(&s)
				.create();
			if (!_engine)
				throw CompilationException(s);

			/* Parse the main function signature. */

			vector< pair<string, char> > arguments;
			char returntype;

			{
				std::istringstream signaturestream(signature);
				L lexer(signaturestream);

				parse_type_signature(lexer, arguments, returntype);
				expect_eof(lexer);
			}

			/* Create the toplevel function from this signature. */

			llvm::FunctionType* ft = llvm::FunctionType::get(
					llvm::Type::getVoidTy(_context),
					false);

			_function = llvm::Function::Create(ft,
					llvm::Function::ExternalLinkage,
					"Calculon Function", _module);

			_toplevel = llvm::BasicBlock::Create(_context, "entry", _function);

//			_builder.SetInsertPoint(_toplevel);

			ASTConstant c(42);

			Compiler compiler(*this);
			compiler.builder.SetInsertPoint(_toplevel);
			compiler.builder.CreateRet(c.codegen(compiler));

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
