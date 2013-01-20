#ifndef CALCULON_INTRINSICS_H
#define CALCULON_INTRINSICS_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class StandardSymbolTable : public MultipleSymbolTable
{
	class UnaryRealIntrinsic : public IntrinsicFunctionSymbol
	{
		string _base;

	public:
		UnaryRealIntrinsic(const string& name, const string& base):
			IntrinsicFunctionSymbol(name, 1),
			_base(base)
		{
		}

		void typeCheckParameter(CompilerState& state,
					int index, llvm::Value* argument, char type)
		{
			if (argument->getType() != state.realType)
				typeError(state, index, argument, type);
		}

		llvm::Type* returnType(const vector<llvm::Type*>& inputTypes)
		{
			return inputTypes[0];
		}

		string intrinsicName(const vector<llvm::Type*>& inputTypes)
		{
			llvm::Type* t = inputTypes[0];
			if (t->isFloatTy())
				return _base + ".f32";
			else if (t->isDoubleTy())
				return _base + ".f64";
			assert(false);
		}
	};

#if 0
	class UnaryRealExternal : public IntrinsicFunctionSymbol
	{
		string _floatname;
		string _doublename;

	public:
		UnaryRealExternal(const string& name, const string& floatname,
				const string& doublename):
			IntrinsicFunctionSymbol(name, 1),
			_floatname(floatname),
			_doublename(doublename)
		{
		}

		void typeCheckParameter(CompilerState& state,
					int index, llvm::Value* argument, char type)
		{
			if (argument->getType() != state.realType)
				typeError(state, index, argument, type);
		}

		llvm::Type* returnType(const vector<llvm::Type*>& inputTypes)
		{
			return inputTypes[0];
		}

		string intrinsicName(const vector<llvm::Type*>& inputTypes)
		{
			llvm::Type* t = inputTypes[0];
			if (t->isFloatTy())
				return _floatname;
			else if (t->isDoubleTy())
				return _doublename;
			assert(false);
		}
	};
#endif

	UnaryRealIntrinsic _sqrt;
	UnaryRealIntrinsic _sin;
	UnaryRealIntrinsic _cos;
	UnaryRealIntrinsic _exp;
	UnaryRealIntrinsic _exp2;
	UnaryRealIntrinsic _log;
	UnaryRealIntrinsic _log10;
	UnaryRealIntrinsic _log2;
#if 0
	UnaryRealIntrinsic _abs;
	UnaryRealIntrinsic _floor;
	UnaryRealIntrinsic _ceil;
	UnaryRealIntrinsic _trunc;
#endif

public:
	StandardSymbolTable():
#if 0
		_sqrt("sqrt", "sqrtf", "sqrt")
#endif
		_sqrt("sqrt", "llvm.sqrt"),
		_sin("sin", "llvm.sin"),
		_cos("cos", "llvm.cos"),
		_exp("exp", "llvm.exp"),
		_exp2("exp2", "llvm.exp2"),
		_log("log", "llvm.log"),
		_log10("log10", "llvm.log10"),
		_log2("log2", "llvm.log2")
#if 0
		_abs("abs", "llvm.fabs"),
		_floor("floor", "llvm.floor"),
		_ceil("ceil", "llvm.ceil"),
		_trunc("trunc", "llvm.trunc")
#endif
	{
		add(&_sqrt);
		add(&_sin);
		add(&_cos);
		add(&_exp);
		add(&_exp2);
		add(&_log);
		add(&_log10);
		add(&_log2);
#if 0
		add(&_abs);
		add(&_floor);
		add(&_ceil);
		add(&_trunc);
#endif
	}
};

#endif
