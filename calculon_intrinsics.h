#ifndef CALCULON_INTRINSICS_H
#define CALCULON_INTRINSICS_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class StandardSymbolTable : public MultipleSymbolTable
{
	using MultipleSymbolTable::add;

	class AddMethod : public BitcodeRealOrVectorHomogeneousSymbol
	{
	public:
		AddMethod():
			BitcodeRealOrVectorHomogeneousSymbol("method +", 2)
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateFAdd(parameters[0], parameters[1]);
		}
	}
	_addMethod;

	class SubMethod : public BitcodeRealOrVectorHomogeneousSymbol
	{
	public:
		SubMethod():
			BitcodeRealOrVectorHomogeneousSymbol("method -", -1)
		{
		}

		void checkParameterCount(CompilerState& state,
				const vector<llvm::Value*>& parameters, int count)
		{
			if ((parameters.size() == 1) || (parameters.size() == 2))
				return;
			BitcodeRealOrVectorHomogeneousSymbol::checkParameterCount(
					state, parameters, count);
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			switch (parameters.size())
			{
				case 1:
					return state.builder.CreateFNeg(parameters[0]);

				case 2:
					return state.builder.CreateFSub(parameters[0], parameters[1]);
			}
			assert(false);
		}
	}
	_subMethod;

	class MulMethod : public BitcodeSymbol
	{
		using CallableSymbol::typeError;

	public:
		MulMethod():
			BitcodeSymbol("method *", 2)
		{
		}

		void typeCheckParameter(CompilerState& state,
					int index, llvm::Value* argument, char type)
		{
			llvm::Type* t = argument->getType();
			switch (index)
			{
				case 1:
					if ((t != state.realType) && (t != state.vectorType))
						typeError(state, index, argument, type);
					break;

				case 2:
					if (t != state.realType)
						typeError(state, index, argument, type);
					break;

				default:
					assert(false);
			}
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return inputTypes[0];
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Value* lhs = parameters[0];
			llvm::Value* rhs = parameters[1];
			if ((lhs->getType() == state.vectorType) && (rhs->getType() == state.realType))
			{
				llvm::Value* v = llvm::UndefValue::get(state.vectorType);
				v = state.builder.CreateInsertElement(v, rhs, state.xindex);
				v = state.builder.CreateInsertElement(v, rhs, state.yindex);
				v = state.builder.CreateInsertElement(v, rhs, state.zindex);
				rhs = v;
			}

			return state.builder.CreateFMul(lhs, rhs);
		}
	}
	_mulMethod;

	class XMethod : public BitcodeVectorSymbol
	{
	public:
		XMethod():
			BitcodeVectorSymbol("method x")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateExtractElement(
					parameters[0], state.xindex);
		}
	}
	_xMethod;

	class YMethod : public BitcodeVectorSymbol
	{
	public:
		YMethod():
			BitcodeVectorSymbol("method y")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateExtractElement(
					parameters[0], state.xindex);
		}
	}
	_yMethod;

	class ZMethod : public BitcodeVectorSymbol
	{
	public:
		ZMethod():
			BitcodeVectorSymbol("method z")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateExtractElement(
					parameters[0], state.xindex);
		}
	}
	_zMethod;

	class SimpleRealExternal : public IntrinsicFunctionSymbol
	{
		using Symbol::name;
		using CallableSymbol::typeError;

	public:
		SimpleRealExternal(const string& name, int params):
			IntrinsicFunctionSymbol(name, params)
		{
		}

		void typeCheckParameter(CompilerState& state,
					int index, llvm::Value* argument, char type)
		{
			if (argument->getType() != state.realType)
				typeError(state, index, argument, type);
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return inputTypes[0];
		}

		string intrinsicName(const vector<llvm::Type*>& inputTypes)
		{
			llvm::Type* t = inputTypes[0];
			const char* suffix = S::chooseDoubleOrFloat("", "f");
			return name + suffix;
		}
	};

	#define REAL1(n) SimpleRealExternal(_##n);
	#define REAL2(n) SimpleRealExternal(_##n);
	#define REAL3(n) SimpleRealExternal(_##n);
	#include "calculon_libm.h"
	#undef REAL1
	#undef REAL2
	#undef REAL3
	char _dummy;

public:
	StandardSymbolTable():
		#define REAL1(n) _##n(#n, 1),
		#define REAL2(n) _##n(#n, 2),
		#define REAL3(n) _##n(#n, 3),
		#include "calculon_libm.h"
		#undef REAL1
		#undef REAL2
		#undef REAL3
		_dummy(0)
	{
		add(&_addMethod);
		add(&_subMethod);
		add(&_mulMethod);
		add(&_xMethod);
		add(&_yMethod);
		add(&_zMethod);

		#define REAL1(n) add(&_##n);
		#define REAL2(n) add(&_##n);
		#define REAL3(n) add(&_##n);
		#include "calculon_libm.h"
		#undef REAL1
		#undef REAL2
		#undef REAL3
	}
};

#endif
