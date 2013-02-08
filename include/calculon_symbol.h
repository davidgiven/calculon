/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_SYMBOL_H
#define CALCULON_SYMBOL_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class CallableSymbol;
class ValuedSymbol;
class VariableSymbol;
class FunctionSymbol;

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

	virtual FunctionSymbol* isFunction()
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

	virtual VariableSymbol* isVariable()
	{
		return NULL;
	}
};

class VariableSymbol : public ValuedSymbol
{
public:
	Type* type;
	FunctionSymbol* function;
	string hash;

public:
	VariableSymbol(const string& name, Type* type):
		ValuedSymbol(name),
		type(type)
	{
		std::stringstream s;
		s << (uintptr_t)this;
		hash = s.str();
	}

	VariableSymbol* isVariable()
	{
		return this;
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

	void checkParameterCount(CompilerState& state, int calledwith, int required)
	{
		if (calledwith != required)
		{
			std::stringstream s;
			s << "attempt to call function '" << name <<
					"' with the wrong number of parameters";
			throw CompilationException(state.position.formatError(s.str()));
		}
	}

	virtual void checkParameterCount(CompilerState& state, int calledwith) = 0;

	void typeError(CompilerState& state,
			int index, llvm::Value* argument, Type* type)
	{
		Type* at = state.types->find(argument->getType());

		std::stringstream s;
		s << "call to parameter " << index << " of function '" << name
				<< "' with wrong type; got " << at->name;
		throw CompilationException(state.position.formatError(s.str()));
	}

	void vectorSizeError(CompilerState& state, VectorType* vectortype)
	{
		std::stringstream s;
		s << "this doesn't make sense for a vector with "
		  << vectortype->size << " element";
		if (vectortype->size != 1)
			s << "s";

		throw CompilationException(state.position.formatError(s.str()));
	}

	virtual void typeCheckParameter(CompilerState& state,
			int index, llvm::Value* argument, Type* type)
	{
		if (type && (argument->getType() != type->llvm))
			typeError(state, index, argument, type);
	}

	virtual llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters) = 0;
};

class FunctionSymbol : public CallableSymbol
{
public:
	const vector<VariableSymbol*> arguments;
	const Type* returntype;
	llvm::Function* function;
	FunctionSymbol* parent; // parent function in the static scope

	typedef map<VariableSymbol*, VariableSymbol*> LocalsMap;
	LocalsMap locals; // maps root variable -> local variable

private:
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

public:
	FunctionSymbol(const string& name, const vector<VariableSymbol*>& arguments,
			Type* returntype):
		CallableSymbol(name),
		arguments(arguments),
		returntype(returntype),
		function(NULL),
		parent(NULL)
	{
		for (typename vector<VariableSymbol*>::const_iterator i = arguments.begin(),
				e = arguments.end(); i != e; i++)
		{
			VariableSymbol* symbol = *i;
			locals[symbol] = symbol;
		}
	}

	FunctionSymbol* isFunction()
	{
		return this;
	}

	void checkParameterCount(CompilerState& state, int calledwith)
	{
		CallableSymbol::checkParameterCount(state, calledwith, arguments.size());
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		/* Type-check the parameters. (Only the formal arguments.) */

		int i = 1;
		typename vector<VariableSymbol*>::const_iterator ai = arguments.begin();
		vector<llvm::Value*>::const_iterator pi = parameters.begin();
		while (ai != arguments.end())
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

	VariableSymbol* importUpvalue(CompilerState& compiler, VariableSymbol* symbol)
	{
		/* If this is already imported --- or is a local --- just use the
		 * variable we already have.
		 */

		typename LocalsMap::const_iterator i = locals.find(symbol);
		if (i != locals.end())
			return i->second;

		/* We need to import this symbol from the next function further up
		 * in the static scope chain.
		 */

		if (!parent)
		{
			std::stringstream s;
			s << "could not import " << symbol << " (" << symbol->name << ")";
			throw CompilationException(s.str());
		}

		assert(parent);
		parent->importUpvalue(compiler, symbol);
		VariableSymbol* localsymbol = compiler.retain(
				new VariableSymbol(symbol->name, symbol->type));
		locals[symbol] = localsymbol;

		return localsymbol;
	}

};

class BitcodeSymbol : public CallableSymbol
{
	int arguments;

public:
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

	BitcodeSymbol(const string& name, int arguments):
		CallableSymbol(name),
		arguments(arguments)
	{
	}

	void checkParameterCount(CompilerState& state, int calledwith)
	{
		CallableSymbol::checkParameterCount(state, calledwith, arguments);
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
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

		return emitBitcode(state, parameters);
	}

	virtual llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes) = 0;
	virtual llvm::Value* emitBitcode(CompilerState& state,
			const vector<llvm::Value*>& parameters) = 0;
};

class ExternalFunctionSymbol : public CallableSymbol
{
	vector<string> inputtypenames;
	string returntypename;
	void (*pointer)();

public:
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

	ExternalFunctionSymbol(const string& name, const vector<string>& inputtypes,
			string returntype, void (*pointer)()):
		CallableSymbol(name),
		inputtypenames(inputtypes),
		returntypename(returntype),
		pointer(pointer)
	{
	}

	void checkParameterCount(CompilerState& state, int calledwith)
	{
		CallableSymbol::checkParameterCount(state, calledwith, inputtypenames.size());
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
		int i = 0;
		vector<llvm::Value*>::const_iterator pi = parameters.begin();
		vector<llvm::Value*> llvmvalues;
		vector<llvm::Type*> llvmtypes;

		Type* returntype = state.types->find(returntypename);
		assert(returntype);
		llvm::Type* externalreturntype = returntype->llvmx;

		/* If we're returning a vector, insert the return pointer now.
		 */

		if (returntype->asVector())
		{
			llvm::Value* p = state.builder.CreateAlloca(
					returntype->asVector()->llvm,
					llvm::ConstantInt::get(state.intType, 1));

			llvmvalues.push_back(p);
			llvmtypes.push_back(p->getType());

			externalreturntype = llvm::Type::getVoidTy(state.context);
		}

		/* Convert and add any other parameters. */

		while (pi != parameters.end())
		{
			llvm::Value* value = *pi;
			Type* internalctype = state.types->find(inputtypenames[i]);
			assert(internalctype);
			typeCheckParameter(state, i+1, value, internalctype);

			if (internalctype->asVector())
			{
				llvm::Value* p = state.builder.CreateAlloca(
						internalctype->asVector()->llvm,
						llvm::ConstantInt::get(state.intType, 1));
				internalctype->asVector()->storeToArray(value, p);
				value = p;
			}
			else
				value = internalctype->convertToExternal(value);

			llvmvalues.push_back(value);
			llvmtypes.push_back(value->getType());

			i++;
			pi++;
		}

		llvm::FunctionType* ft = llvm::FunctionType::get(
				externalreturntype, llvmtypes, false);

		/* Create the function. */

		llvm::Constant* iptr = llvm::ConstantInt::get(
				state.engine->getDataLayout()->getIntPtrType(state.context, 0),
				(uint64_t) pointer);
		llvm::Value* fptr = llvm::ConstantExpr::getIntToPtr(iptr,
				llvm::PointerType::get(ft, 0));

		llvm::Value* retval = state.builder.CreateCall(fptr, llvmvalues);
		if (returntype->asVector())
			retval = returntype->asVector()->loadFromArray(llvmvalues[0]);
		else
			retval = returntype->convertToInternal(retval);
		return retval;
	}
};

class BitcodeBooleanSymbol : public BitcodeSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeBooleanSymbol(string id, int parameters):
		BitcodeSymbol(id, parameters)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		Type* at = state.types->find(argument->getType());
		if (!at->equals(state.booleanType))
			typeError(state, index, argument, type);
	}
};

class BitcodeRealSymbol : public BitcodeSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeRealSymbol(string id, int parameters):
		BitcodeSymbol(id, parameters)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		if (argument->getType() != state.realType->llvm)
			typeError(state, index, argument, type);
	}
};

class BitcodeRealComparisonSymbol : public BitcodeSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeRealComparisonSymbol(string id):
		BitcodeSymbol(id, 2)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		if (argument->getType() != state.realType->llvm)
			typeError(state, index, argument, type);
	}

	llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes)
	{
		return state.booleanType->llvm;
	}
};

class BitcodeVectorSymbol : public BitcodeSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeVectorSymbol(string id):
		BitcodeSymbol(id, 1)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		if (!state.types->find(argument->getType())->asVector())
			typeError(state, index, argument, type);
	}
};

class BitcodeRealOrVectorSymbol : public BitcodeSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeRealOrVectorSymbol(string id, int parameters):
		BitcodeSymbol(id, parameters)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		Type* at = state.types->find(argument->getType());
		if (!at->equals(state.realType) && !at->asVector())
			typeError(state, index, argument, type);
	}

	llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes)
	{
		return inputTypes[0];
	}
};

class BitcodeHomogeneousSymbol : public BitcodeSymbol
{
	llvm::Type* firsttype;

	using Symbol::name;
public:
	BitcodeHomogeneousSymbol(string id, int parameters):
		BitcodeSymbol(id, parameters),
		firsttype(NULL)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		if (index == 1)
		{
			firsttype = argument->getType();
		}
		else
		{
			if (argument->getType() != firsttype)
			{
				std::stringstream s;
				s << "parameters to " << name
						<< " are not all the same type";
				throw CompilationException(state.position.formatError(s.str()));
			}
		}
	}
};

class BitcodeComparisonSymbol : public BitcodeHomogeneousSymbol
{
public:
	BitcodeComparisonSymbol(string id):
		BitcodeHomogeneousSymbol(id, 2)
	{
	}

	llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes)
	{
		return state.booleanType->llvm;
	}
};

class BitcodeRealOrVectorHomogeneousSymbol : public BitcodeHomogeneousSymbol
{
	using CallableSymbol::typeError;

public:
	BitcodeRealOrVectorHomogeneousSymbol(string id, int parameters):
		BitcodeHomogeneousSymbol(id, parameters)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		Type* at = state.types->find(argument->getType());
		if (!at->equals(state.realType) && !at->asVector())
			typeError(state, index, argument, type);

		return BitcodeHomogeneousSymbol::typeCheckParameter(state, index,
				argument, type);
	}

	llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes)
	{
		return inputTypes[0];
	}
};

class BitcodeRealOrVectorArraySymbol : public BitcodeSymbol
{
	Type* _firsttype;

	using CallableSymbol::typeError;

public:
	BitcodeRealOrVectorArraySymbol(const string& id, int parameters):
		BitcodeSymbol(id, parameters)
	{
	}

	void typeCheckParameter(CompilerState& state,
				int index, llvm::Value* argument, Type* type)
	{
		Type* t = state.types->find(argument->getType());
		switch (index)
		{
			case 1:
				_firsttype = t;
				if (!t->equals(state.realType) && !t->asVector())
					typeError(state, index, argument, type);
				break;

			default:
				if (_firsttype->asVector() && t->equals(_firsttype))
					break;
				if (t->equals(state.realType))
					break;

				typeError(state, index, argument, type);
				break;
		}
	}

	llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes)
	{
		return inputTypes[0];
	}

	llvm::Value* convertRHS(CompilerState& state, llvm::Value* lhs,
			llvm::Value* rhs)
	{
		Type* lhst = state.types->find(lhs->getType());
		Type* rhst = state.types->find(rhs->getType());
		if (lhst->asVector() && (rhst == state.realType))
		{
			VectorType* lhsvt = lhst->asVector();
			llvm::Value* v = llvm::UndefValue::get(lhst->llvm);

			for (unsigned i = 0; i < lhsvt->size; i++)
				v = lhsvt->setElement(v, i, rhs);

			rhs = v;
		}

		return rhs;
	}
};

class IntrinsicFunctionSymbol : public CallableSymbol
{
	int arguments;

public:
	using CallableSymbol::typeCheckParameter;
	using CallableSymbol::typeError;

	IntrinsicFunctionSymbol(const string& name, int arguments):
		CallableSymbol(name),
		arguments(arguments)
	{
	}

	void checkParameterCount(CompilerState& state, int calledwith)
	{
		CallableSymbol::checkParameterCount(state, calledwith, arguments);
	}

	llvm::Value* emitCall(CompilerState& state,
			const vector<llvm::Value*>& parameters)
	{
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
				returnType(state, llvmtypes), llvmtypes, false);

		llvm::Constant* f = state.module->getOrInsertFunction(
				intrinsicName(llvmtypes), ft,
				llvm::AttrListPtr::get(state.context,
					llvm::AttributeWithIndex::get(
							state.context,
							llvm::AttrListPtr::FunctionIndex,
							llvm::Attributes::ReadNone)));

		return state.builder.CreateCall(f, parameters);
	}

	virtual llvm::Type* returnType(CompilerState& state,
			const vector<llvm::Type*>& inputTypes) = 0;
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
