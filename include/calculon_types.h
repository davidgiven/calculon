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
	llvm::Type* llvmstruct;
	llvm::Type* llvmpointer;

	using Type::state;

public:
	VectorType(CompilerState& state, const string& name):
		Type(state, name)
	{
		llvm::Type* t = state.types->find("real")->llvm;
		this->llvm = llvm::VectorType::get(t, 4);

		this->llvmstruct = llvm::StructType::get(t, t, t, NULL);
		this->llvmpointer = llvm::PointerType::get(this->llvmstruct, 0);

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

	void storeToArray(llvm::Value* value, llvm::Value* pointer) const
	{
		llvm::Value* xv = state.builder.CreateExtractElement(value, state.xindex);
		llvm::Value* yv = state.builder.CreateExtractElement(value, state.yindex);
		llvm::Value* zv = state.builder.CreateExtractElement(value, state.zindex);

		state.builder.CreateStore(xv, state.builder.CreateStructGEP(pointer, 0));
		state.builder.CreateStore(yv, state.builder.CreateStructGEP(pointer, 1));
		state.builder.CreateStore(zv, state.builder.CreateStructGEP(pointer, 2));
	}

	llvm::Value* loadFromArray(llvm::Value* pointer) const
	{
		llvm::Value* xv = state.builder.CreateLoad(state.builder.CreateStructGEP(pointer, 0));
		llvm::Value* yv = state.builder.CreateLoad(state.builder.CreateStructGEP(pointer, 1));
		llvm::Value* zv = state.builder.CreateLoad(state.builder.CreateStructGEP(pointer, 2));

		llvm::Value* v = llvm::UndefValue::get(this->llvm);
		v = state.builder.CreateInsertElement(v, xv, state.xindex);
		v = state.builder.CreateInsertElement(v, yv, state.yindex);
		v = state.builder.CreateInsertElement(v, zv, state.zindex);
		return v;
	}

};

class TypeRegistry
{
private:
	CompilerState& _compiler;

	typedef map<string, Type*> ByNameMap;
	ByNameMap _byname;

	typedef map<llvm::Type*, Type*> ByLLVMMap;
	ByLLVMMap _byllvm;

public:
	TypeRegistry(CompilerState& state):
		_compiler(state)
	{
	}

	void addType(Type* type)
	{
		_byname[type->name] = type;
		_byllvm[type->llvm] = type;
	}

	Type* find(const string& name)
	{
		typename ByNameMap::const_iterator i = _byname.find(name);
		if (i != _byname.end())
			return i->second;

		Type* type;
		if (name == "real")
			type = _compiler.retain(new RealType(_compiler, name));
		else if (name == "boolean")
			type = _compiler.retain(new BooleanType(_compiler, name));
		else if (name == "vector")
			type = _compiler.retain(new VectorType(_compiler, name));
		else if (name == "!float")
			type = _compiler.retain(new FloatType(_compiler, name));
		else if (name == "!double")
			type = _compiler.retain(new DoubleType(_compiler, name));
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
		throw NULL;
	}
};
#endif
