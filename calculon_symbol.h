#ifndef CALCULON_SYMBOL_H
#define CALCULON_SYMBOL_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

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

	virtual llvm::Value* value(llvm::Module* module) = 0;
};

class VariableSymbol : public Symbol
{
	llvm::Value* _value;
	char _type;

public:
	VariableSymbol(const string& name, char type):
		Symbol(name),
		_value(NULL),
		_type(type)
	{
	}

	void setValue(llvm::Value* value)
	{
		_value = value;
	}

	llvm::Value* value(llvm::Module* module)
	{
		return _value;
	}

	char type() const
	{
		return _type;
	}
};

class FunctionSymbol : public Symbol
{
	llvm::Value* _value;

public:
	FunctionSymbol(const string& name):
		Symbol(name),
		_value(NULL)
	{
	}

	void setValue(llvm::Value* value)
	{
		_value = value;
	}

	llvm::Value* value(llvm::Module* module)
	{
		return _value;
	}
};

class SymbolTable : public Object
{
	SymbolTable* _next;

public:
	SymbolTable():
		_next(NULL)
	{
	}

	SymbolTable(SymbolTable& next):
		_next(&next)
	{
	}

	virtual ~SymbolTable()
	{
	}

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
	SingletonSymbolTable(SymbolTable& next):
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

	MultipleSymbolTable(SymbolTable& next):
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
