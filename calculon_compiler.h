#ifndef CALCULON_COMPILER_H
#define CALCULON_COMPILER_H

#ifndef CALCULON_H
#error Don't include this, include calculon.h instead.
#endif

template <typename Real>
class Compiler
{
public:
	llvm::LLVMContext& context;
	llvm::IRBuilder<> builder;
	llvm::Type* realType;
	llvm::Type* vectorType;
	llvm::Type* structType;

private:
	class ASTNode;
	typedef set<ASTNode*> Nodes;
	Nodes _nodes;

public:
	Compiler(llvm::LLVMContext& context):
		context(context),
		builder(context)
	{
		realType = createRealType((Real) 0.0);
		vectorType = llvm::VectorType::get(realType, 4);
		structType = llvm::StructType::get(
				realType, realType, realType, NULL);
	}

	~Compiler()
	{
		for (typename Nodes::const_iterator i = _nodes.begin(),
				e = _nodes.end(); i != e; i++)
			delete *i;
	}

	template <class T> T& node(auto_ptr<T> ptr)
	{
		T* p = ptr.get();
		_nodes.add(p);
		ptr.release();
		return *p;
	};

	void parse(std::istream& signaturestream, std::istream& codestream)
	{
	}

private:
	class SaveState
	{
		Compiler& _compiler;
		llvm::BasicBlock* _bb;
		llvm::BasicBlock::iterator _i;

	public:
		SaveState(Compiler& compiler):
			_compiler(compiler),
			_bb(compiler.builder.GetInsertBlock()),
			_i(compiler.builder.GetInsertPoint())
		{
		}

		~SaveState()
		{
			_compiler.builder.SetInsertPoint(_bb, _i);
		}
	};

private:
	llvm::Type* createRealType(double)
	{
		return llvm::Type::getDoubleTy(context);
	}

	llvm::Type* createRealType(float)
	{
		return llvm::Type::getFloatTy(context);
	}

	struct ASTNode
	{
		ASTNode& parent;

		ASTNode(ASTNode& parent):
			parent(parent)
		{
		}

		virtual ~ASTNode()
		{
		}

		virtual llvm::Value* codegen(Compiler& compiler) = 0;
	};

	struct ASTConstant : public ASTNode
	{
		Real value;

		ASTConstant(ASTNode& parent, Real value):
			ASTNode(parent),
			value(value)
		{
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			return llvm::ConstantFP::get(compiler.context,
					llvm::APFloat(value));
		}
	};

	struct ASTVector : public ASTNode
	{
		ASTNode& x;
		ASTNode& y;
		ASTNode& z;

		ASTVector(ASTNode& parent, ASTNode& x, ASTNode& y, ASTNode& z):
			ASTNode(parent),
			x(x), y(y), z(z)
		{
		}

		llvm::Value* codegen(Compiler& compiler)
		{

		}
	};

	struct ASTFunction : public ASTNode
	{
		ASTNode& definition;
		ASTNode& body;
		llvm::Function* _function;

		ASTFunction(ASTNode& parent, ASTNode& definition, ASTNode& body):
			ASTNode(parent),
			definition(definition),
			body(body),
			_function(NULL)
		{
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			/* Compile the function definition itself. */

			{
				typename Compiler::SaveState state;

#if 0
				llvm::BasicBlock* toplevel = llvm::BasicBlock::Create(
						compiler, "", _function);
				compiler.SetInsertPoint(toplevel);
#endif

				llvm::Value* v = body.codegen(compiler);
				compiler.builder.CreateRet(v);
			}

			return body.codegen(compiler);
		}
	};
};

#endif
