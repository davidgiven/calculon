#ifndef CALCULON_SYMBOL_H
#define CALCULON_SYMBOL_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class CallableSymbol;
class ValuedSymbol;

class Symbol : public Object
{
	string _name;

public:
	Symbol(const string& name):
		_name(name)
	{
	}

	virtual ~Symbol()
	{
	}

	const string& name() const
	{
		return _name;
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
	llvm::Value* _value;

public:
	ValuedSymbol(const string& name):
		Symbol(name),
		_value(NULL)
	{
	}

	ValuedSymbol* isValued()
	{
		return this;
	}

	void setValue(llvm::Value* value)
	{
		_value = value;
	}

	llvm::Value* value() const
	{
		return _value;
	}
};

class VariableSymbol : public ValuedSymbol
{
	char _type;

public:
	VariableSymbol(const string& name, char type):
		ValuedSymbol(name),
		_type(type)
	{
	}

	char type() const
	{
		return _type;
	}
};

class CallableSymbol : public Symbol
{
public:
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
			s << "attempt to call function '" << name() <<
					"' with the wrong number of parameters";
			throw CompilationException(state.position.formatError(s.str()));
		}
	}

	void typeError(CompilerState& state,
			int index, llvm::Value* argument, char type)
	{
		std::stringstream s;
		s << "call to parameter " << index << " of function '" << name()
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
	vector<VariableSymbol*> _arguments;
	char _returntype;
	llvm::Function* _function;

public:
	FunctionSymbol(const string& name, const vector<VariableSymbol*>& arguments,
			char returntype):
		CallableSymbol(name),
		_arguments(arguments),
		_returntype(returntype)
	{
	}

	vector<VariableSymbol*>& arguments()
	{
		return _arguments;
	}

	char returntype()
	{
		return _returntype;
	}

	void setFunction(llvm::Function* f)
	{
		_function = f;
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		checkParameterCount(state, parameters, _arguments.size());

		int i = 1;
		vector<llvm::Value*>::const_iterator pi = parameters.begin();
		vector<VariableSymbol*>::const_iterator ai = _arguments.begin();
		while (pi != parameters.end())
		{
			llvm::Value* v = *pi;
			typeCheckParameter(state, i, v, (*ai)->type());

			i++;
			pi++;
			ai++;
		}

		assert(_function);
		return state.builder.CreateCall(_function, parameters);
	}
};

class IntrinsicFunctionSymbol : public CallableSymbol
{
	int _arguments;

public:
	IntrinsicFunctionSymbol(const string& name, int arguments):
		CallableSymbol(name),
		_arguments(arguments)
	{
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		checkParameterCount(state, parameters, _arguments);

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
		if (_symbol && (_symbol->name() == name))
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
		_symbols[symbol->name()] = symbol;
	}

	Symbol* resolve(const string& name)
	{
		Symbols::const_iterator i = _symbols.find(name);
		if (i == _symbols.end())
			return SymbolTable::resolve(name);
		return i->second;
	}
};

#endif
