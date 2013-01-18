#ifndef CALCULON_COMPILER_H
#define CALCULON_COMPILER_H

#ifndef CALCULON_H
#error Don't include this, include calculon.h instead.
#endif

template <typename Real>
class CompilerBase
{
};

template <>
class CompilerBase<double>
{
protected:
	enum
	{
		REAL = 'D'
	};

	static llvm::Type* createRealType(llvm::LLVMContext& context)
	{
		return llvm::Type::getDoubleTy(context);
	}
};

template <>
class CompilerBase<float>
{
protected:
	enum
	{
		REAL = 'F'
	};

	static llvm::Type* createRealType(llvm::LLVMContext& context)
	{
		return llvm::Type::getFloatTy(context);
	}
};

template <typename Real>
class Compiler : public CompilerBase<Real>
{
public:
	llvm::LLVMContext& context;
	llvm::Module* module;
	llvm::IRBuilder<> builder;
	llvm::Type* intType;
	llvm::Value* xindex;
	llvm::Value* yindex;
	llvm::Value* zindex;
	llvm::Type* realType;
	llvm::Type* vectorType;
	llvm::Type* structType;

private:
	class ASTNode;
	typedef set<ASTNode*> Nodes;
	Nodes _nodes;

	typedef Lexer<Real> L;

	enum
	{
		VECTOR = 'V'
	};
	using CompilerBase<Real>::REAL;
	using CompilerBase<Real>::createRealType;

public:
	Compiler(llvm::LLVMContext& context, llvm::Module* module):
		context(context),
		module(module),
		builder(context)
	{
		intType = llvm::IntegerType::get(context, 32);
		xindex = llvm::ConstantInt::get(intType, 0);
		xindex = llvm::ConstantInt::get(intType, 1);
		xindex = llvm::ConstantInt::get(intType, 2);
		realType = createRealType(context);
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

	template <class T> T& node(T* p)
	{
		auto_ptr<T> ptr(p); // ensure deletion on exception
		_nodes.insert(p); // might throw
		ptr.release();
		return *p;
	};

public:
	void compile(std::istream& signaturestream, std::istream& codestream)
	{
		vector< pair<string, char> > arguments;
		char returntype;

		L signaturelexer(signaturestream);
		parse_type_signature(signaturelexer, arguments, returntype);
		expect_eof(signaturelexer);

		L codelexer(codestream);
		ASTToplevel& ast = parse_toplevel(codelexer);

		llvm::Function* f = static_cast<llvm::Function*>(ast.codegen(*this));
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

	llvm::Value* check_real(llvm::Value* v)
	{
		if (v->getType() != realType)
			throw CompilationException("type mismatch: expected a real");
		return v;
	}

private:
	class ASTFrame;
	class ASTFunction;

	struct NodeVisitor
	{
		virtual void visit(ASTNode& node) {}
		virtual void visit(ASTFrame& frame) {}
		virtual void visit(ASTFunction& function) {}
	};

	struct ASTNode
	{
		ASTNode* parent;

		ASTNode():
			parent(NULL)
		{
		}

		virtual ~ASTNode()
		{
		}

		virtual llvm::Value* codegen(Compiler& compiler) = 0;

		llvm::Value* codegen_to_real(Compiler& compiler)
		{
			return compiler.check_real(codegen(compiler));
		}

		virtual void visit(NodeVisitor& visitor)
		{
			visitor.visit(*this);
		}
	};

	struct ASTConstant : public ASTNode
	{
		Real value;

		ASTConstant(Real value):
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

		ASTVector(ASTNode& x, ASTNode& y, ASTNode& z):
			x(x), y(y), z(z)
		{
			x.parent = y.parent = z.parent = this;
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = llvm::UndefValue::get(compiler.vectorType);

			llvm::Value* xv = x.codegen_to_real(compiler);
			llvm::Value* yv = y.codegen_to_real(compiler);
			llvm::Value* zv = z.codegen_to_real(compiler);

			v = compiler.builder.CreateInsertElement(v, xv,
					llvm::ConstantInt::get(compiler.intType, 0));
			v = compiler.builder.CreateInsertElement(v, yv,
					llvm::ConstantInt::get(compiler.intType, 1));
			v = compiler.builder.CreateInsertElement(v, zv,
					llvm::ConstantInt::get(compiler.intType, 2));

			return v;
		}

		void visit(NodeVisitor& visitor)
		{
			ASTNode::visit(visitor);
			visitor.visit(*this);
			x.visit(visitor);
			y.visit(visitor);
			z.visit(visitor);
		}
	};

	struct ASTFrame : public ASTNode
	{
		ASTFrame()
		{
		}

		void visit(NodeVisitor& visitor)
		{
			ASTNode::visit(visitor);
			visitor.visit(*this);
		}
	};

	struct ASTFunction : public ASTFrame
	{
		ASTNode& definition;
		ASTNode& body;
		llvm::Function* _function;

		ASTFunction(ASTNode& definition, ASTNode& body):
			definition(definition),
			body(body),
			_function(NULL)
		{
			definition.parent = body.parent = this;
		}

		void visit(NodeVisitor& visitor)
		{
			ASTFrame::visit(visitor);
			visitor.visit(*this);
			definition.visit(visitor);
			body.visit(visitor);
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

	struct ASTToplevel : public ASTFrame
	{
		ASTNode& body;

		ASTToplevel(ASTNode& body):
			body(body)
		{
			body.parent = this;
		}

		void visit(NodeVisitor& visitor)
		{
			ASTFrame::visit(visitor);
			visitor.visit(*this);
			body.visit(visitor);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::FunctionType* ft = llvm::FunctionType::get(
					llvm::Type::getVoidTy(compiler.context),
					false);

			llvm::Function* f = llvm::Function::Create(ft,
					llvm::Function::InternalLinkage,
					"toplevel", compiler.module);

			llvm::BasicBlock* bb = llvm::BasicBlock::Create(compiler.context,
					"", f);

			compiler.builder.SetInsertPoint(bb);
			llvm::Value* v = body.codegen(compiler);
			compiler.builder.CreateRet(v);

			return f;
		}
	};

private:
	void expect(L& lexer, int token)
	{
		if (lexer.token() != token)
		{
			std::stringstream s;
			s << "expected " << token;
			lexer.error(s.str());
		}
		lexer.next();
	}

	void expect_operator(L& lexer, const string& s)
	{
		if ((lexer.token() != L::OPERATOR) || (lexer.id() != s))
			lexer.error(string("expected '"+s+"'"));
		lexer.next();
	}

	void expect_eof(L& lexer)
	{
		if (lexer.token() != L::EOF)
			lexer.error("expected EOF");
	}

	void parse_identifier(L& lexer, string& id)
	{
		if (lexer.token() != L::IDENTIFIER)
			lexer.error("expected identifier");

		id = lexer.id();
		lexer.next();
	}

	void parse_list_separator(L& lexer)
	{
		switch (lexer.token())
		{
			case L::COMMA:
				lexer.next();
				break;

			case L::CLOSEPAREN:
				break;

			default:
				lexer.error("expected comma or close parenthesis");
		}
	}

	void parse_typespec(L& lexer, char& type)
	{
		expect(lexer, L::COLON);

		if (lexer.token() != L::IDENTIFIER)
			lexer.error("expected a type name");

		if (lexer.id() == "vector")
			type = VECTOR;
		else if (lexer.id() == "real")
			type = REAL;
		else
			lexer.error("expected a type name");

		lexer.next();
	}

	void parse_type_signature(L& lexer, vector<pair<string, char> >& arguments,
			char& returntype)
	{
		expect(lexer, L::OPENPAREN);

		while (lexer.token() != L::CLOSEPAREN)
		{
			std::cerr << "token " << lexer.token() << "\n";

			string id;
			char type = REAL;

			parse_identifier(lexer, id);

			if (lexer.token() == L::COLON)
				parse_typespec(lexer, type);

			arguments.push_back(pair<string, char>(id, type));
			parse_list_separator(lexer);
		}

		expect(lexer, L::CLOSEPAREN);

		returntype = REAL;
		if (lexer.token() == L::COLON)
			parse_typespec(lexer, returntype);
	}

	ASTNode& parse_leaf(L& lexer)
	{
		switch (lexer.token())
		{
			case L::NUMBER:
			{
				Real value = lexer.real();
				lexer.next();
				return node(new ASTConstant(value));
			}

			case L::OPERATOR:
			{
				const string& id = lexer.id();
				if (id == "<")
					return parse_vector(lexer);
				break;
			}
		}

		lexer.error("expected an expression");
		throw NULL; // do not return
	}

	ASTVector& parse_vector(L& lexer)
	{
		expect_operator(lexer, "<");
		ASTNode& x = parse_leaf(lexer);
		expect(lexer, L::COMMA);
		ASTNode& y = parse_leaf(lexer);
		expect(lexer, L::COMMA);
		ASTNode& z = parse_leaf(lexer);
		expect_operator(lexer, ">");

		return node(new ASTVector(x, y, z));
	}

	ASTToplevel& parse_toplevel(L& lexer)
	{
		ASTNode& body = parse_leaf(lexer);
		return node(new ASTToplevel(body));
	}
};

#endif
