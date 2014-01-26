/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_H
#define CALCULON_H

#if !defined(CALCULON_LLVM)
	#define CALCULON_LLVM 33
#endif

#include <stdexcept>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <cassert>
#include <cctype>
#include <memory>
#include <boost/aligned_storage.hpp>
#include <boost/static_assert.hpp>
#include <boost/algorithm/string/split.hpp>

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Attributes.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

namespace Calculon
{
	using std::string;
	using std::vector;
	using std::map;
	using std::pair;
	using std::set;
	using std::auto_ptr;

	namespace Impl
	{
		class SettingsBase
		{
		};
	}

	class RealIsDouble : public Impl::SettingsBase
	{
	public:
		typedef double Real;

	public:
		static llvm::Type* createRealType(llvm::LLVMContext& context)
		{
			return llvm::Type::getDoubleTy(context);
		}

		template <typename T>
		static T chooseDoubleOrFloat(T d, T f)
		{
			return d;
		}
	};

	class RealIsFloat : public Impl::SettingsBase
	{
	public:
		typedef float Real;

	public:
		static llvm::Type* createRealType(llvm::LLVMContext& context)
		{
			return llvm::Type::getFloatTy(context);
		}

		template <typename T>
		static T chooseDoubleOrFloat(T d, T f)
		{
			return f;
		}
	};

	#include "calculon_allocator.h"

	namespace Impl
	{
		template <class S, int size>
		struct Vector
		{
			BOOST_STATIC_ASSERT(size > 0);
			BOOST_STATIC_ASSERT(size >= 4);

			enum
			{
				LENGTH = size * sizeof(typename S::Real),
				ALIGN = LENGTH
			};

			union
			{
				typename S::Real m[size];
				typename boost::aligned_storage<LENGTH, ALIGN>::type _align;
			};
		};

		template <class S>
		struct Vector<S, 1>
		{
			enum
			{
				LENGTH = 1 * sizeof(typename S::Real),
				ALIGN = LENGTH
			};

			union
			{
				typename S::Real m[1];
				struct
				{
					typename S::Real x;
					typename boost::aligned_storage<LENGTH, ALIGN>::type _align;
				};
			};
		};

		template <class S>
		struct Vector<S, 2>
		{
			enum
			{
				LENGTH = 2 * sizeof(typename S::Real),
				ALIGN = LENGTH
			};

			union
			{
				typename S::Real m[2];
				struct
				{
					typename S::Real x, y;
					typename boost::aligned_storage<LENGTH, ALIGN>::type _align;
				};
			};
		};

		template <class S>
		struct Vector<S, 3>
		{
			enum
			{
				LENGTH = 4 * sizeof(typename S::Real),
				ALIGN = LENGTH
			};

			union
			{
				typename S::Real m[3];
				struct
				{
					typename S::Real x, y, z;
					typename boost::aligned_storage<LENGTH, ALIGN>::type _align;
				};
			};
		};

		template <class S>
		struct Vector<S, 4>
		{
			enum
			{
				LENGTH = 4 * sizeof(typename S::Real),
				ALIGN = LENGTH
			};

			union
			{
				typename S::Real m[4];
				struct
				{
					typename S::Real x, y, z, w;
					typename boost::aligned_storage<LENGTH, ALIGN>::type _align;
				};
			};
		};
	}

	template <class S>
	class Instance
	{
	public:
		typedef typename S::Real Real;
		template <int size> struct Vector : public Impl::Vector<S, size>
		{
		};

	private:
		class CompilationException : public std::invalid_argument
		{
		public:
			CompilationException(const string& what):
				std::invalid_argument(what)
			{
			}
		};

		struct Position
		{
			int line;
			int column;

			string formatError(const string& what)
			{
				std::stringstream s;
				s << what
					<< " at " << line << ":" << column;
				return s.str();
			}
		};

		class TypeRegistry;
		class Type;
		class VectorType;

		class CompilerState : public Allocator
		{
		public:
			llvm::LLVMContext& context;
			llvm::Module* module;
			llvm::IRBuilder<> builder;
			llvm::ExecutionEngine* engine;
			Position position;
			TypeRegistry* types;
			llvm::Type* intType;
			Type* realType;
			llvm::Type* doubleType;
			llvm::Type* floatType;
			Type* booleanType;

			CompilerState(llvm::LLVMContext& context, llvm::Module* module,
					llvm::ExecutionEngine* engine):
				context(context),
				module(module),
				builder(context),
				engine(engine),
				types(NULL),
				intType(NULL),
				realType(NULL), doubleType(NULL), floatType(NULL)
			{
			}
		};

		#include "calculon_symbol.h"
		#include "calculon_types.h"
	private:
		#include "calculon_lexer.h"
	public:
		#include "calculon_intrinsics.h"
	private:
		#include "calculon_compiler.h"

	public:
		template <typename FuncType>
		class Program
		{
		private:
			llvm::LLVMContext _context;
			SymbolTable& _symbols;
			llvm::Module* _module;
			llvm::ExecutionEngine* _engine;
			llvm::Function* _function;
			FuncType* _funcptr;

		public:
			typedef typename S::Real Real;

		public:
			Program(SymbolTable& symbols, const string& code, const string& signature,
						const map<string, string>& typealiases):
					_symbols(symbols),
					_funcptr(NULL)
			{
				std::istringstream stream(code);
				init(stream, signature, typealiases);
			}

			Program(SymbolTable& symbols, const string& code, const string& signature):
					_symbols(symbols),
					_funcptr(NULL)
			{
				std::istringstream stream(code);
				map<string, string> typealiases;
				init(stream, signature, typealiases);
			}

			Program(SymbolTable& symbols, std::istream& code, const string& signature,
						const map<string, string>& typealiases):
					_symbols(symbols),
					_funcptr(NULL)
			{
				init(code, signature, typealiases);
			}

			Program(SymbolTable& symbols, std::istream& code, const string& signature):
					_symbols(symbols),
					_funcptr(NULL)
			{
				map<string, string> typealiases;
				init(code, signature, typealiases);
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
			void init(std::istream& codestream, const string& signature,
					const map<string, string>& typealiases)
			{
				_module = new llvm::Module("Calculon Function", _context);

				llvm::InitializeNativeTarget();

				llvm::TargetOptions options;
//				options.PrintMachineCode = true;
				options.UnsafeFPMath = true;
				options.RealignStack = true;
				options.LessPreciseFPMADOption = true;
				options.GuaranteedTailCallOpt = true;
				options.AllowFPOpFusion = llvm::FPOpFusion::Fast;

				string s;
				_engine = llvm::EngineBuilder(_module)
					.setErrorStr(&s)
					.setOptLevel(llvm::CodeGenOpt::Aggressive)
					.setTargetOptions(options)
					.create();
				if (!_engine)
					throw CompilationException(s);
				_engine->DisableLazyCompilation();
	//			_engine->DisableSymbolSearching();

				Compiler compiler(_context, _module, _engine, typealiases);

				/* Compile the program. */

				std::istringstream signaturestream(signature);
				ToplevelSymbol* f = compiler.compile(signaturestream, codestream,
						&_symbols);
				_function = f->function;


				generate_machine_code();
			}

		private:

			void generate_machine_code()
			{
				//_module->dump();
				llvm::verifyFunction(*_function);

				llvm::FunctionPassManager fpm(_module);
				llvm::PassManager mpm;
				llvm::PassManagerBuilder pmb;
				pmb.OptLevel = 3;
				pmb.populateFunctionPassManager(fpm);

				pmb.Inliner = llvm::createFunctionInliningPass(275);
				pmb.populateModulePassManager(mpm);

				fpm.doInitialization();
				fpm.run(*_function);
				mpm.run(*_module);

				_funcptr = (FuncType*) _engine->getPointerToFunction(_function);
				assert(_funcptr);
			}
		};
	};
}

#endif
