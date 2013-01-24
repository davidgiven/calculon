#ifndef CALCULON_SYMBOL_H
#define CALCULON_SYMBOL_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class CallableSymbol;
class ValuedSymbol;

class Symbol : public Object
{
public:
	const string name;

public:
	Symbol(const string& name):
		name(name)
	{
	}

	virtual ~Symbol()
	{
	}

	virtual ValuedSymbol* isValued()
	{
		return NULL;
	}

	virtual CallableSymbol* isCallable()
	{
		return NULL;
	}
};

class ValuedSymbol : public Symbol
{
public:
	llvm::Value* value;

public:
	ValuedSymbol(const string& name):
		Symbol(name),
		value(NULL)
	{
	}

	ValuedSymbol* isValued()
	{
		return this;
	}
};

class VariableSymbol : public ValuedSymbol
{
public:
	const char type;

public:
	VariableSymbol(const string& name, char type):
		ValuedSymbol(name),
		type(type)
	{
	}
};

class CallableSymbol : public Symbol
{
public:
	using Symbol::name;

	CallableSymbol(const string& name):
		Symbol(name)
	{
	}

	CallableSymbol* isCallable()
	{
		return this;
	}

	void checkParameterCount(CompilerState& state,
			const vector<llvm::Value*>& parameters, int count)
	{
		if (parameters.size() != count)
		{
			std::stringstream s;
			s << "attempt to call function '" << name <<
					"' with the wrong number of parameters";
			throw CompilationException(state.position.formatError(s.str()));
		}
	}

	void typeError(CompilerState& state,
			int index, llvm::Value* argument, char type)
	{
		std::stringstream s;
		s << "call to parameter " << index << " of function '" << name
				<< "' with wrong type";
		throw CompilationException(state.position.formatError(s.str()));
	}

	virtual void typeCheckParameter(CompilerState& state,
			int index, llvm::Value* argument, char type)
	{
		if (argument->getType() != state.getInternalType(type))
			typeError(state, index, argument, type);
	}

	virtual llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters) = 0;
};

class FunctionSymbol : public CallableSymbol
{
public:
	const vector<VariableSymbol*> arguments;
	const char returntype;
	llvm::Function* function;

	using CallableSymbol::checkParameterCount;
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

public:
	FunctionSymbol(const string& name, const vector<VariableSymbol*>& arguments,
			char returntype):
		CallableSymbol(name),
		arguments(arguments),
		returntype(returntype),
		function(NULL)
	{
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		checkParameterCount(state, parameters, arguments.size());

		int i = 1;
		vector<llvm::Value*>::const_iterator pi = parameters.begin();
		typename vector<VariableSymbol*>::const_iterator ai = arguments.begin();
		while (pi != parameters.end())
		{
			llvm::Value* v = *pi;
			typeCheckParameter(state, i, v, (*ai)->type);

			i++;
			pi++;
			ai++;
		}

		assert(function);
		return state.builder.CreateCall(function, parameters);
	}
};

class IntrinsicFunctionSymbol : public CallableSymbol
{
	int arguments;

public:
	using CallableSymbol::checkParameterCount;
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

	IntrinsicFunctionSymbol(const string& name, int arguments):
		CallableSymbol(name),
		arguments(arguments)
	{
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		checkParameterCount(state, parameters, arguments);

		int i = 1;
		vector<llvm::Value*>::const_iterator pi = parameters.begin();
		vector<llvm::Type*> llvmtypes;
		while (pi != parameters.end())
		{
			llvm::Value* v = *pi;
			typeCheckParameter(state, i, v, 0);
			llvmtypes.push_back(v->getType());

			i++;
			pi++;
		}

		llvm::FunctionType* ft = llvm::FunctionType::get(
				returnType(llvmtypes), llvmtypes, false);

		llvm::Function* f = llvm::Function::Create(ft,
				llvm::Function::ExternalLinkage,
				intrinsicName(llvmtypes), state.module);
		f->setDoesNotAccessMemory();

		return state.builder.CreateCall(f, parameters);
	}

	virtual llvm::Type* returnType(const vector<llvm::Type*>& inputTypes) = 0;
	virtual string intrinsicName(const vector<llvm::Type*>& inputTypes) = 0;
};

class SymbolTable : public Object
{
	SymbolTable* _next;

public:
	SymbolTable():
		_next(NULL)
	{
	}

	SymbolTable(SymbolTable* next):
		_next(next)
	{
	}

	virtual ~SymbolTable()
	{
	}

	virtual void add(Symbol* symbol) = 0;

	virtual Symbol* resolve(const string& name)
	{
		if (_next)
			return _next->resolve(name);
		return NULL;
	}
};

class SingletonSymbolTable : public SymbolTable
{
	Symbol* _symbol;

public:
	SingletonSymbolTable(SymbolTable* next):
		SymbolTable(next),
		_symbol(NULL)
	{
	}

	void add(Symbol* symbol)
	{
		assert(_symbol == NULL);
		_symbol = symbol;
	}

	Symbol* resolve(const string& name)
	{
		if (_symbol && (_symbol->name == name))
			return _symbol;
		return SymbolTable::resolve(name);
	}
};

class MultipleSymbolTable : public SymbolTable
{
	typedef map<string, Symbol*> Symbols;
	Symbols _symbols;

public:
	MultipleSymbolTable()
	{
	}

	MultipleSymbolTable(SymbolTable* next):
		SymbolTable(next)
	{
	}

	void add(Symbol* symbol)
	{
		_symbols[symbol->name] = symbol;
	}

	Symbol* resolve(const string& name)
	{
		typename Symbols::const_iterator i = _symbols.find(name);
		if (i == _symbols.end())
			return SymbolTable::resolve(name);
		return i->second;
	}
};

#endif
