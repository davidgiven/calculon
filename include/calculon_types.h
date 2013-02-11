/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_TYPES_H
#define CALCULON_TYPES_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class VectorType;
class RealType;

class Type : public Object
{
public:
	CompilerState& state;
	const string name;
	llvm::Type* llvm; /* LLVM type used inside Calculon code */
	llvm::Type* llvmx; /* LLVM type used to communicate with C code */

	Type(CompilerState& state, const string& name):
		state(state),
		name(name),
		llvm(NULL),
		llvmx(NULL)
	{
	}

	bool equals(const Type* other)
	{
		return llvm == other->llvm;
	}

	virtual RealType* asReal()
	{
		return NULL;
	}

	virtual VectorType* asVector()
	{
		return NULL;
	}

	virtual const VectorType* asVector() const
	{
		return NULL;
	}

	virtual llvm::Value* convertToExternal(llvm::Value* value)
	{
		return value;
	}

	virtual llvm::Value* convertToInternal(llvm::Value* value)
	{
		return value;
	}
};

class RealType : public Type
{
	llvm::Type* _llvmdouble;
	llvm::Type* _llvmfloat;

public:
	using Type::state;
	using Type::llvm;
	using Type::llvmx;

public:
	RealType(CompilerState& state, const string& name):
		Type(state, name)
	{
		llvm = llvmx = S::createRealType(state.context);

		_llvmdouble = llvm::Type::getDoubleTy(state.context);
		_llvmfloat = llvm::Type::getFloatTy(state.context);
	}

	RealType* asReal()
	{
		return NULL;
	}

	llvm::Value* convertToInternal(llvm::Value* value)
	{
		llvm::Type* t = value->getType();
		if (t == llvm)
			return value;

		if (t->getPrimitiveSizeInBits() > llvm->getPrimitiveSizeInBits())
			return state.builder.CreateFPTrunc(value, llvm);
		else
			return state.builder.CreateFPExt(value, llvm);
	}
};

class DoubleType : public RealType
{
public:
	using Type::state;
	using Type::llvmx;

public:
	DoubleType(CompilerState& state, const string& name):
		RealType(state, name)
	{
		llvmx = llvm::Type::getDoubleTy(state.context);
	}

	llvm::Value* convertToExternal(llvm::Value* value)
	{
		return state.builder.CreateFPExt(value, llvmx);
	}
};

class FloatType : public RealType
{
public:
	using Type::state;
	using Type::llvmx;

public:
	FloatType(CompilerState& state, const string& name):
		RealType(state, name)
	{
		llvmx = llvm::Type::getFloatTy(state.context);
	}

	llvm::Value* convertToExternal(llvm::Value* value)
	{
		return state.builder.CreateFPTrunc(value, llvmx);
	}
};

class BooleanType : public Type
{
public:
	BooleanType(CompilerState& state, const string& name):
		Type(state, name)
	{
		this->llvm = this->llvmx = llvm::IntegerType::get(state.context, 1);
	}
};

class VectorType : public Type
{
public:
	unsigned size;
	llvm::Type* llvmpointer;

	using Type::state;

public:
	VectorType(CompilerState& state, const string& name, unsigned size):
		Type(state, name),
		size(size)
	{
		llvm::Type* t = state.types->find("real")->llvm;
		this->llvm = llvm::VectorType::get(t, size);

		vector<llvm::Type*> elements;
		elements.insert(elements.begin(), size, t);

		this->llvmpointer = llvm::PointerType::get(this->llvm, 0);

		this->llvmx = this->llvmpointer;
	}

	VectorType* asVector()
	{
		return this;
	}

	const VectorType* asVector() const
	{
		return this;
	}

	llvm::Value* convertToExternal(llvm::Value* value)
	{
		assert(false && "should not be called on vector");
		return NULL;
	}

	llvm::Value* convertToInternal(llvm::Value* value)
	{
		assert(false && "should not be called on vector");
		return NULL;
	}

	llvm::Value* getElement(llvm::Value* vector, unsigned index) const
	{
		assert(index < size);
		return state.builder.CreateExtractElement(vector,
				llvm::ConstantInt::get(state.intType, index));
	}

	llvm::Value* setElement(llvm::Value* vector, unsigned index, llvm::Value* v) const
	{
		assert(index < size);
		return state.builder.CreateInsertElement(vector, v,
				llvm::ConstantInt::get(state.intType, index));
	}

	void storeToArray(llvm::Value* value, llvm::Value* pointer) const
	{
		state.builder.CreateStore(value, pointer);
	}

	llvm::Value* loadFromArray(llvm::Value* pointer) const
	{
		return state.builder.CreateLoad(pointer);
	}

};

class TypeRegistry
{
private:
	CompilerState& _compiler;

	typedef map<string, string> ExtraTypesMap;
	const ExtraTypesMap& _extratypes;

	typedef map<string, Type*> ByNameMap;
	ByNameMap _byname;

	typedef map<llvm::Type*, Type*> ByLLVMMap;
	ByLLVMMap _byllvm;

public:
	TypeRegistry(CompilerState& state, const ExtraTypesMap& extratypes):
		_compiler(state),
		_extratypes(extratypes)
	{
	}

	void addType(Type* type)
	{
		_byname[type->name] = type;
		_byllvm[type->llvm] = type;
	}

	Type* find(string name)
	{
		typename ExtraTypesMap::const_iterator ei = _extratypes.find(name);
		if (ei != _extratypes.end())
			name = ei->second;

		typename ByNameMap::const_iterator i = _byname.find(name);
		if (i != _byname.end())
			return i->second;

		Type* type;
		if (name == "real")
			type = _compiler.retain(new RealType(_compiler, name));
		else if (name == "boolean")
			type = _compiler.retain(new BooleanType(_compiler, name));
		else if (name == "!float")
			type = _compiler.retain(new FloatType(_compiler, name));
		else if (name == "!double")
			type = _compiler.retain(new DoubleType(_compiler, name));
		else if (name == "vector")
			type = _compiler.retain(new VectorType(_compiler, name, 3));
		else if (name.substr(0, 7) == "vector*")
			type = _compiler.retain(new VectorType(_compiler, name,
					atoi(name.c_str() + 7)));
		else
			return NULL;

		addType(type);
		return type;
	}

	Type* find(llvm::Type* llvmtype)
	{
		typename ByLLVMMap::const_iterator i = _byllvm.find(llvmtype);
		if (i != _byllvm.end())
			return i->second;

		assert(false && "no mapping between LLVM type and Calculon type");
		throw 0;
	}
};
#endif
