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

class Type : public Object
{
public:
	const string name;
	llvm::Type* llvm; /* LLVM type used inside Calculon code */
	llvm::Type* llvmx; /* LLVM type used to communicate with C code */

	Type(const string name, CompilerState& compiler):
		name(name),
		llvm(NULL),
		llvmx(NULL)
	{
	}

	virtual VectorType* asVector()
	{
		return NULL;
	}
};

class RealType : public Type
{
public:
	RealType(const string name, CompilerState& compiler):
		Type(name, compiler)
	{
		this->llvm = this->llvmx = S::createRealType(compiler.context);
	}
};

class BooleanType : public Type
{
public:
	BooleanType(const string name, CompilerState& compiler):
		Type(name, compiler)
	{
		this->llvm = this->llvmx = llvm::IntegerType::get(compiler.context, 1);
	}
};

class VectorType : public Type
{
public:
	llvm::Type* llvmstruct;
	llvm::Type* llvmpointer;

public:
	VectorType(const string name, CompilerState& compiler):
		Type(name, compiler)
	{
		llvm::Type* t = compiler.types->find("real")->llvm;
		this->llvm = llvm::VectorType::get(t, 4);

		this->llvmstruct = llvm::StructType::get(t, t, t, NULL);
		this->llvmpointer = llvm::PointerType::get(this->llvmstruct, 0);

		this->llvmx = this->llvmpointer;
	}

	VectorType* asVector()
	{
		return this;
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
	TypeRegistry(CompilerState& compiler):
		_compiler(compiler)
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
			type = _compiler.retain(new RealType(name, _compiler));
		else if (name == "boolean")
			type = _compiler.retain(new BooleanType(name, _compiler));
		else if (name == "vector")
			type = _compiler.retain(new VectorType(name, _compiler));
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
