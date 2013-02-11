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

	class AddMethod : public BitcodeRealOrVectorArraySymbol
	{
		using BitcodeRealOrVectorArraySymbol::convertRHS;

	public:
		AddMethod():
			BitcodeRealOrVectorArraySymbol("method +", 2)
		{
		}

		llvm::Value* emitBitcode(CompilerState& state,
					const vector<llvm::Value*>& parameters)
		{
			llvm::Value* lhs = parameters[0];
			llvm::Value* rhs = parameters[1];
			rhs = convertRHS(state, lhs, rhs);

			return state.builder.CreateFAdd(lhs, rhs);
		}
	}
	_addMethod;

	class SubMethod : public BitcodeRealOrVectorArraySymbol
	{
		using BitcodeRealOrVectorArraySymbol::convertRHS;

	public:
		SubMethod():
			BitcodeRealOrVectorArraySymbol("method -", -1)
		{
		}

		void checkParameterCount(CompilerState& state, int calledwith)
		{
			/* Accept one or two parameters. */
			if ((calledwith == 1) || (calledwith == 2))
				return;

			/* Otherwise, let the superclass produce the error. */
			BitcodeRealOrVectorArraySymbol::checkParameterCount(
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
				{
					llvm::Value* lhs = parameters[0];
					llvm::Value* rhs = parameters[1];
					rhs = convertRHS(state, lhs, rhs);

					return state.builder.CreateFSub(lhs, rhs);
				}
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

	class SumMethod : public BitcodeVectorSymbol
	{
	public:
		SumMethod():
			BitcodeVectorSymbol("method sum")
		{
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType->llvm;
		}

	private:
		llvm::Value* sum_power_of_2(CompilerState& state, llvm::Value* source,
				int minelement, int maxelement)
		{
			int isize = maxelement - minelement;

			if (isize == 1)
			{
				/* Just return the first value. */

				return state.builder.CreateExtractElement(source,
						llvm::ConstantInt::get(state.intType, minelement+0));
			}
			else if (isize == 2)
			{
				/* This vector is sufficiently small that we might as well
				 * just grab the elements and add them as scalars.
				 */

				llvm::Value* v1 = state.builder.CreateExtractElement(source,
						llvm::ConstantInt::get(state.intType, minelement+0));
				llvm::Value* v2 = state.builder.CreateExtractElement(source,
						llvm::ConstantInt::get(state.intType, minelement+1));
				return state.builder.CreateFAdd(v1, v2);
			}
			else
			{
				/* This vector is big enough --- four elements or more ---
				 * that we're going to use Magic Vector Tricks to sum it
				 * in log(n) time.
				 */

				int isize = maxelement - minelement;
				assert(isize >= 2);
				int osize = isize / 2;

				/* Split the elements in the source into two vectors of half the
				 * size.
				 */

				vector<llvm::Constant*> mask1array;
				vector<llvm::Constant*> mask2array;
				for (int i = 0; i < osize; i++)
				{
					mask1array.push_back(
							llvm::ConstantInt::get(state.intType, i + minelement));
					mask2array.push_back(
							llvm::ConstantInt::get(state.intType, i + osize + minelement));
				}

				llvm::Value* mask1 = llvm::ConstantVector::get(mask1array);
				llvm::Value* mask2 = llvm::ConstantVector::get(mask2array);

				llvm::Value* v1 = state.builder.CreateShuffleVector(source,
						llvm::UndefValue::get(source->getType()), mask1);
				llvm::Value* v2 = state.builder.CreateShuffleVector(source,
						llvm::UndefValue::get(source->getType()), mask2);

				llvm::Value* v = state.builder.CreateFAdd(v1, v2);

				/* Now sum the vector we've just created (recursively). */

				return sum_power_of_2(state, v, 0, osize);
			}
		}

		int find_power_of_2(int i)
		{
			int j = 1;

			while (i > 1)
			{
				i >>= 1;
				j <<= 1;
			}

			return j;
		}

		llvm::Value* sum_non_power_of_2(CompilerState& state, llvm::Value* source,
				int minelement, int maxelement)
		{
			vector<llvm::Value*> results;

			while (minelement != maxelement)
			{
				int size = maxelement - minelement;
				int pow2 = find_power_of_2(size);

				results.push_back(sum_power_of_2(state, source, minelement, minelement+pow2));
				minelement += pow2;
			}

			if (results.size() == 1)
				return results[0];

			if (results.size() == 2)
				return state.builder.CreateFAdd(results[0], results[1]);

			/* There are many results, so marshal them back into a vector and
			 * try again.
			 */

			llvm::Type* desttype = llvm::VectorType::get(state.realType->llvm,
					results.size());
			llvm::Value* v = llvm::UndefValue::get(desttype);

			for (unsigned i = 0; i < results.size(); i++)
			{
				v = state.builder.CreateInsertElement(v, results[i],
					llvm::ConstantInt::get(state.intType, i));
			}

			return sum_non_power_of_2(state, v, 0, results.size());
		}

	public:
		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			llvm::Value* value = parameters[0];
			VectorType* vtype = state.types->find(value->getType())->asVector();
			int size = vtype->size;

			return sum_non_power_of_2(state, parameters[0], 0, size);
		}
	}
	_sumMethod;

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
			VectorAccessorMethod(3, 4, 2, "z")
		{
		}
	}
	_zMethod;

	class WMethod : public VectorAccessorMethod
	{
	public:
		WMethod():
			VectorAccessorMethod(1, 4, 3, "w")
		{
		}
	}
	_wMethod;

	class VectorSquareBracketMethod : public BitcodeSymbol
	{
		using CallableSymbol::vectorSizeError;
		using CallableSymbol::typeError;

	public:
		VectorSquareBracketMethod():
			BitcodeSymbol("method []", -1)
		{
		}

		void checkParameterCount(CompilerState& state, int calledwith)
		{
			/* Accept one or two parameters. */
			if ((calledwith == 2) || (calledwith == 3))
				return;

			/* Otherwise, let the superclass produce the error. */
			BitcodeSymbol::checkParameterCount(state, calledwith);
		}

		void typeCheckParameter(CompilerState& state,
					int index, llvm::Value* argument, Type* type)
		{
			Type* t = state.types->find(argument->getType());

			switch (index)
			{
				case 1:
					if (!t->asVector())
						typeError(state, index, argument, type);
					break;

				default:
					if (!t->equals(state.realType))
						typeError(state, index, argument, type);
					break;
			}
		}

		llvm::Type* returnType(CompilerState& state,
				const vector<llvm::Type*>& inputTypes)
		{
			return state.realType->llvm;
		}

		llvm::Value* emitBitcode(CompilerState& state,
				const vector<llvm::Value*>& parameters)
		{
			llvm::Value* vector = parameters[0];
			VectorType* t = state.types->find(vector->getType())->asVector();

			llvm::Value* element;
			switch (parameters.size())
			{
				case 2:
				{
					element = state.builder.CreateFPToUI(parameters[1],
							state.intType);
					break;
				}

				case 3:
				{
					/* Square vectors */

					unsigned root = (unsigned)sqrt(t->size);
					if ((root*root) != t->size)
					{
						std::stringstream s;
						s << "you can only use a .mXY operator on square vectors, and this one has "
						  << t->size << " element";
						if (t->size != 1)
							s << "s";

						throw CompilationException(state.position.formatError(s.str()));
					}

					llvm::Value* x = state.builder.CreateFPToUI(parameters[1],
							state.intType);
					llvm::Value* y = state.builder.CreateFPToUI(parameters[2],
							state.intType);
					element = state.builder.CreateMul(y,
							llvm::ConstantInt::get(state.intType, root));
					element = state.builder.CreateAdd(element, x);
					break;
				}

				default:
					assert(false);
			}

			element = state.builder.CreateURem(element,
					llvm::ConstantInt::get(state.intType, t->size));
			return state.builder.CreateExtractElement(vector, element);
		}
	}
	_vectorSquareBracketMethod;

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

private:
	void malformed_function_signature(Lexer& lexer, const string& what)
	{
		throw CompilationException(lexer.position().formatError(what));
	}

	string parse_typespec(Lexer& lexer)
	{
		typedef Lexer L;

		if (lexer.token() != L::IDENTIFIER)
			malformed_function_signature(lexer, "expected type name");

		std::stringstream s;
		s << lexer.id();
		lexer.next();

		if ((lexer.token() == L::OPERATOR) && (lexer.id() == "*"))
		{
			s << "*";
			lexer.next();

			if (lexer.token() != L::NUMBER)
				lexer.error("invalid n-vector type specifier");
			int size = (int)lexer.real();
			if ((Real)size != lexer.real())
				lexer.error("n-vector size must be an integer");
			if (size <= 0)
				lexer.error("n-vector size must be greater than 0");
			lexer.next();

			s << size;
		}

		string type = s.str();
		if (type == "float")
			return "!float";
		if (type == "double")
			return "!double";
		return type;
	}

public:
	/* Registers an external function. */

	void add(const string& name, const string& signature, void (*ptr)())
	{
		std::stringstream stream(signature);
		Lexer lexer(stream);
		typedef Lexer L;

		if (lexer.token() != L::OPENPAREN)
			malformed_function_signature(lexer, "expected '('");
		lexer.next();

		vector<string> inputtypes;
		while (lexer.token() != L::CLOSEPAREN)
		{
			inputtypes.push_back(parse_typespec(lexer));
			if ((lexer.token() != L::CLOSEPAREN) && (lexer.token() != L::COMMA))
				malformed_function_signature(lexer, "expected ',' or ')'");
			if (lexer.token() == L::COMMA)
				lexer.next();
		}
		lexer.next();

		if (lexer.token() != L::COLON)
			malformed_function_signature(lexer, "expected ':'");
		lexer.next();

		string returntype = parse_typespec(lexer);

		add(retain(new ExternalFunctionSymbol(name, inputtypes, returntype, ptr)));
	}

	/* Registers a real global variable. */

	void add(const string& name, double value)
	{
		add(retain(new ExternalRealConstantSymbol(name, value)));
	}

	/* Registers a vector global variable. */

	void add(const string& name, const vector<double>& value)
	{
		add(retain(new ExternalVectorConstantSymbol(name, value)));
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
		add(&_sumMethod);
		add(&_xMethod);
		add(&_yMethod);
		add(&_zMethod);
		add(&_wMethod);
		add(&_vectorSquareBracketMethod);

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
