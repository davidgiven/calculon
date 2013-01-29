/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_INTRINSICS_H
#define CALCULON_INTRINSICS_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class StandardSymbolTable : public MultipleSymbolTable, public Allocator
{
	using MultipleSymbolTable::add;

	class NotMethod : public BitcodeBooleanSymbol
	{
	public:
		NotMethod():
			BitcodeBooleanSymbol("method not", 1)
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
			return state.builder.CreateNot(parameters[0]);
		}
	}
	_notMethod;

	class LTMethod : public BitcodeRealComparisonSymbol
	{
	public:
		LTMethod():
			BitcodeRealComparisonSymbol("method <")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateFCmpOLT(parameters[0], parameters[1]);
		}
	}
	_ltMethod;

	class LEMethod : public BitcodeRealComparisonSymbol
	{
	public:
		LEMethod():
			BitcodeRealComparisonSymbol("method <=")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateFCmpOLE(parameters[0], parameters[1]);
		}
	}
	_leMethod;

	class GTMethod : public BitcodeRealComparisonSymbol
	{
	public:
		GTMethod():
			BitcodeRealComparisonSymbol("method >")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateFCmpOGT(parameters[0], parameters[1]);
		}
	}
	_gtMethod;

	class GEMethod : public BitcodeRealComparisonSymbol
	{
	public:
		GEMethod():
			BitcodeRealComparisonSymbol("method >=")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			return state.builder.CreateFCmpOGE(parameters[0], parameters[1]);
		}
	}
	_geMethod;

	class EQMethod : public BitcodeComparisonSymbol
	{
	public:
		EQMethod():
			BitcodeComparisonSymbol("method ==")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Type* type = parameters[0]->getType();

			if (type == state.realType)
				return state.builder.CreateFCmpOEQ(parameters[0], parameters[1]);
			else if (type == state.booleanType)
				return state.builder.CreateICmpEQ(parameters[0], parameters[1]);
			else if (type == state.vectorType)
			{
				llvm::Value* x0 = state.builder.CreateExtractElement(
						parameters[0], state.xindex);
				llvm::Value* y0 = state.builder.CreateExtractElement(
						parameters[0], state.yindex);
				llvm::Value* z0 = state.builder.CreateExtractElement(
						parameters[0], state.zindex);
				llvm::Value* x1 = state.builder.CreateExtractElement(
						parameters[1], state.xindex);
				llvm::Value* y1 = state.builder.CreateExtractElement(
						parameters[1], state.yindex);
				llvm::Value* z1 = state.builder.CreateExtractElement(
						parameters[1], state.zindex);

				llvm::Value* x = state.builder.CreateFCmpOEQ(x0, x1);
				llvm::Value* y = state.builder.CreateFCmpOEQ(y0, y1);
				llvm::Value* z = state.builder.CreateFCmpOEQ(z0, z1);

				llvm::Value* v = state.builder.CreateAnd(x, y);
				return state.builder.CreateAnd(v, z);
			}
			else
				assert(false);
		}
	}
	_eqMethod;

	class NEMethod : public BitcodeComparisonSymbol
	{
	public:
		NEMethod():
			BitcodeComparisonSymbol("method !=")
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Type* type = parameters[0]->getType();

			if (type == state.realType)
				return state.builder.CreateFCmpONE(parameters[0], parameters[1]);
			else if (type == state.booleanType)
				return state.builder.CreateICmpNE(parameters[0], parameters[1]);
			else if (type == state.vectorType)
			{
				llvm::Value* x0 = state.builder.CreateExtractElement(
						parameters[0], state.xindex);
				llvm::Value* y0 = state.builder.CreateExtractElement(
						parameters[0], state.yindex);
				llvm::Value* z0 = state.builder.CreateExtractElement(
						parameters[0], state.zindex);
				llvm::Value* x1 = state.builder.CreateExtractElement(
						parameters[1], state.xindex);
				llvm::Value* y1 = state.builder.CreateExtractElement(
						parameters[1], state.yindex);
				llvm::Value* z1 = state.builder.CreateExtractElement(
						parameters[1], state.zindex);

				llvm::Value* x = state.builder.CreateFCmpONE(x0, x1);
				llvm::Value* y = state.builder.CreateFCmpONE(y0, y1);
				llvm::Value* z = state.builder.CreateFCmpONE(z0, z1);

				llvm::Value* v = state.builder.CreateAnd(x, y);
				return state.builder.CreateAnd(v, z);
			}
			else
				assert(false);
		}
	}
	_neMethod;

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

		void checkParameterCount(CompilerState& state, int calledwith)
		{
			if ((calledwith == 1) || (calledwith == 2))
				return;
			BitcodeRealOrVectorHomogeneousSymbol::checkParameterCount(
					state, calledwith);
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

	class MulMethod : public BitcodeRealOrVectorArraySymbol
	{
		using BitcodeRealOrVectorArraySymbol::convertRHS;

	public:
		MulMethod():
			BitcodeRealOrVectorArraySymbol("method *", 2)
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Value* lhs = parameters[0];
			llvm::Value* rhs = parameters[1];
			rhs = convertRHS(state, lhs, rhs);

			return state.builder.CreateFMul(lhs, rhs);
		}
	}
	_mulMethod;

	class DivMethod : public BitcodeRealOrVectorArraySymbol
	{
		using BitcodeRealOrVectorArraySymbol::convertRHS;

	public:
		DivMethod():
			BitcodeRealOrVectorArraySymbol("method /", 2)
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Value* lhs = parameters[0];
			llvm::Value* rhs = parameters[1];
			rhs = convertRHS(state, lhs, rhs);

			return state.builder.CreateFDiv(lhs, rhs);
		}
	}
	_divMethod;

	class Length2Method : public BitcodeVectorSymbol
	{
	public:
		Length2Method():
			BitcodeVectorSymbol("method length2")
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
			llvm::Value* v = parameters[0];
			v = state.builder.CreateFMul(v, v);
			llvm::Value* x = state.builder.CreateExtractElement(
					v, state.xindex);
			llvm::Value* y = state.builder.CreateExtractElement(
					v, state.yindex);
			llvm::Value* z = state.builder.CreateExtractElement(
					v, state.zindex);

			v = state.builder.CreateFAdd(x, y);
			v = state.builder.CreateFAdd(v, z);
			return v;
		}
	}
	_length2Method;

	class LengthMethod : public BitcodeVectorSymbol
	{
	public:
		LengthMethod():
			BitcodeVectorSymbol("method length")
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
			llvm::Value* v = parameters[0];
			v = state.builder.CreateFMul(v, v);
			llvm::Value* x = state.builder.CreateExtractElement(
					v, state.xindex);
			llvm::Value* y = state.builder.CreateExtractElement(
					v, state.yindex);
			llvm::Value* z = state.builder.CreateExtractElement(
					v, state.zindex);

			v = state.builder.CreateFAdd(x, y);
			v = state.builder.CreateFAdd(v, z);

			llvm::Constant* f = state.module->getOrInsertFunction(
					S::chooseDoubleOrFloat("llvm.sqrt.f64", "llvm.sqrt.f32"),
					state.realType, NULL);

			return state.builder.CreateCall(f, v);
		}
	}
	_lengthMethod;

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
					parameters[0], state.yindex);
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
					parameters[0], state.zindex);
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

	void add(const string& name, const string& signature, void (*ptr)())
	{
		unsigned i = signature.find('=');
		if (i != 1)
			throw CompilationException("malformed external function signature");

		char returntype = signature[0];
		string inputtypes = signature.substr(2);

		add(retain(new ExternalFunctionSymbol(name, inputtypes, returntype, ptr)));
	}

public:
	template <typename T>
	void add(const string& name, const string& signature, T* ptr)
	{
		add(name, signature, (void (*)()) ptr);
	}

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
		add(&_notMethod);
		add(&_ltMethod);
		add(&_leMethod);
		add(&_gtMethod);
		add(&_geMethod);
		add(&_eqMethod);
		add(&_neMethod);
		add(&_addMethod);
		add(&_subMethod);
		add(&_mulMethod);
		add(&_divMethod);
		add(&_length2Method);
		add(&_lengthMethod);
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
