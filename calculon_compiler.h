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
	llvm::IRBuilder<> builder;
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
	Compiler(llvm::LLVMContext& context):
		context(context),
		builder(context)
	{
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

	template <class T> T& node(auto_ptr<T> ptr)
	{
		T* p = ptr.get();
		_nodes.add(p);
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
	}

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
