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
			return state.realType->llvm;
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
			Type* type = state.types->find(parameters[0]->getType());

			if (type == state.realType)
				return state.builder.CreateFCmpOEQ(parameters[0], parameters[1]);
			else if (type == state.booleanType)
				return state.builder.CreateICmpEQ(parameters[0], parameters[1]);
			else if (type->asVector())
			{
				VectorType* vtype = type->asVector();

				llvm::Value* v = llvm::ConstantInt::getTrue(state.booleanType->llvm);

				for (unsigned i = 0; i < vtype->size; i++)
				{
					llvm::Value* x0 = vtype->getElement(parameters[0], i);
					llvm::Value* x1 = vtype->getElement(parameters[1], i);
					llvm::Value* x = state.builder.CreateFCmpOEQ(x0, x1);
					v = state.builder.CreateAnd(v, x);
				}

				return v;
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
			Type* type = state.types->find(parameters[0]->getType());

			if (type == state.realType)
				return state.builder.CreateFCmpONE(parameters[0], parameters[1]);
			else if (type == state.booleanType)
				return state.builder.CreateICmpNE(parameters[0], parameters[1]);
			else if (type->asVector())
			{
				VectorType* vtype = type->asVector();

				llvm::Value* v = llvm::ConstantInt::getFalse(state.booleanType->llvm);

				for (unsigned i = 0; i < vtype->size; i++)
				{
					llvm::Value* x0 = vtype->getElement(parameters[0], i);
					llvm::Value* x1 = vtype->getElement(parameters[1], i);
					llvm::Value* x = state.builder.CreateFCmpONE(x0, x1);
					v = state.builder.CreateOr(v, x);
				}

				return v;
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
			return state.realType->llvm;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			VectorType* vtype = state.types->find(parameters[0]->getType())->asVector();
			return llvm::ConstantFP::get(state.realType->llvm, vtype->size);
		}
	}
	_lengthMethod;

	class Magnitude2Method : public BitcodeVectorSymbol
	{
	public:
		Magnitude2Method():
			BitcodeVectorSymbol("method magnitude2")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType->llvm;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			VectorType* vtype = state.types->find(parameters[0]->getType())->asVector();

			llvm::Value* v = parameters[0];
			v = state.builder.CreateFMul(v, v);

			llvm::Value* sum = llvm::ConstantFP::get(state.realType->llvm, 0.0);
			for (unsigned i = 0; i < vtype->size; i++)
			{
				llvm::Value* e = vtype->getElement(v, i);
				sum = state.builder.CreateFAdd(sum, e);
			}

			return sum;
		}
	}
	_magnitude2Method;

	class MagnitudeMethod : public BitcodeVectorSymbol
	{
	public:
		MagnitudeMethod():
			BitcodeVectorSymbol("method magnitude")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType->llvm;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			VectorType* vtype = state.types->find(parameters[0]->getType())->asVector();

			llvm::Value* v = parameters[0];
			v = state.builder.CreateFMul(v, v);

			llvm::Value* sum = llvm::ConstantFP::get(state.realType->llvm, 0.0);
			for (unsigned i = 0; i < vtype->size; i++)
			{
				llvm::Value* e = vtype->getElement(v, i);
				sum = state.builder.CreateFAdd(sum, e);
			}

			llvm::Constant* f = state.module->getOrInsertFunction(
					S::chooseDoubleOrFloat("llvm.sqrt.f64", "llvm.sqrt.f32"),
					state.realType->llvm, NULL);

			return state.builder.CreateCall(f, sum);
		}
	}
	_magnitudeMethod;

	class VectorAccessorMethod : public BitcodeVectorSymbol
	{
		unsigned _minelements;
		unsigned _maxelements;
		unsigned _element;

		using CallableSymbol::vectorSizeError;

	public:
		VectorAccessorMethod(unsigned minelements, unsigned maxelements,
				unsigned element, string name):
			BitcodeVectorSymbol("method " + name),
			_minelements(minelements),
			_maxelements(maxelements),
			_element(element)
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType->llvm;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			VectorType* t = state.types->find(parameters[0]->getType())->asVector();
			if ((t->size < _minelements) || (t->size > _maxelements))
				vectorSizeError(state, t);

			return t->getElement(parameters[0], _element);
		}
	};

	class XMethod : public VectorAccessorMethod
	{
	public:
		XMethod():
			VectorAccessorMethod(1, 4, 0, "x")
		{
		}
	}
	_xMethod;

	class YMethod : public VectorAccessorMethod
	{
	public:
		YMethod():
			VectorAccessorMethod(2, 4, 1, "y")
		{
		}
	}
	_yMethod;

	class ZMethod : public VectorAccessorMethod
	{
	public:
		ZMethod():
			VectorAccessorMethod(3, 4, 0, "z")
		{
		}
	}
	_zMethod;

	class WMethod : public VectorAccessorMethod
	{
	public:
		WMethod():
			VectorAccessorMethod(1, 4, 0, "w")
		{
		}
	}
	_wMethod;

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
			if (argument->getType() != state.realType->llvm)
				typeError(state, index, argument, type);
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return inputTypes[0];
		}

		string intrinsicName(const vector<llvm::Type*>& inputTypes)
		{
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

	static string convert_type_char(char c)
	{
		switch (c)
		{
			case 'R': return "real";
			case 'V': return "vector";
			case 'B': return "boolean";
		}

		if (c == 'D')
			return S::chooseDoubleOrFloat("real", "!double");
		if (c == 'F')
			return S::chooseDoubleOrFloat("!float", "real");

		std::stringstream s;
		s << "type char '" << c << "' not recognised";
		throw CompilationException(s.str());
	}

	void add(const string& name, const string& signature, void (*ptr)())
	{
		unsigned i = signature.find('=');
		if (i != 1)
			throw CompilationException("malformed external function signature");

		string returntype = convert_type_char(signature[0]);
		vector<string> inputtypes;
		for (unsigned i = 2; i < signature.size(); i++)
			inputtypes.push_back(convert_type_char(signature[i]));

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
		add(&_lengthMethod);
		add(&_magnitude2Method);
		add(&_magnitudeMethod);
		add(&_xMethod);
		add(&_yMethod);
		add(&_zMethod);
		add(&_wMethod);

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
