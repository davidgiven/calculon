#ifndef CALCULON_COMPILER_H
#define CALCULON_COMPILER_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
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
	class ASTVariable;

	typedef set<ASTNode*> Nodes;
	Nodes _nodes;

	typedef Lexer<Real> L;

	enum
	{
		VECTOR = 'V'
	};
	using CompilerBase<Real>::REAL;
	using CompilerBase<Real>::createRealType;

	class TypeException : public CompilationException
	{
	public:
		TypeException(const string& what, ASTNode& node):
			CompilationException(node.position.formatError(what))
		{
		}
	};

	class SymbolException : public CompilationException
	{
		static string formatError(ASTVariable& node)
		{
			std::stringstream s;
			s << "unresolved symbol '" << node.id << "'";
			return node.position.formatError(s.str());
		}

	public:
		SymbolException(ASTVariable& node):
			CompilationException(formatError(node))
		{
		}
	};

public:
	Compiler(llvm::LLVMContext& context, llvm::Module* module):
		context(context),
		module(module),
		builder(context)
	{
		intType = llvm::IntegerType::get(context, 32);
		xindex = llvm::ConstantInt::get(intType, 0);
		yindex = llvm::ConstantInt::get(intType, 1);
		zindex = llvm::ConstantInt::get(intType, 2);
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
		parse_typesignature(signaturelexer, arguments, returntype);
		expect_eof(signaturelexer);

		MultipleSymbolTable symboltable;

		L codelexer(codestream);
		ASTToplevel& ast = parse_toplevel(codelexer, symboltable);
		ast.resolveVariables();

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

private:
	class ASTFrame;
	class ASTFunction;

	struct ASTNode
	{
		ASTNode* parent;
		Position position;

		ASTNode(const Position& position):
			parent(NULL),
			position(position)
		{
		}

		virtual ~ASTNode()
		{
		}

		virtual llvm::Value* codegen(Compiler& compiler) = 0;

		llvm::Value* codegen_to_real(Compiler& compiler)
		{
			llvm::Value* v = codegen(compiler);

			if (v->getType() != compiler.realType)
				throw TypeException("type mismatch: expected a real", *this);
			return v;
		}

		llvm::Value* codegen_to_vector(Compiler& compiler)
		{
			llvm::Value* v = codegen(compiler);

			if (v->getType() != compiler.vectorType)
				throw TypeException("type mismatch: expected a vector", *this);
			return v;
		}

		virtual void resolveVariables()
		{
		}

		virtual ASTFrame& getFrame()
		{
			return parent->getFrame();
		}
	};

	struct ASTConstant : public ASTNode
	{
		Real value;

		ASTConstant(const Position& position, Real value):
			ASTNode(position),
			value(value)
		{
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			return llvm::ConstantFP::get(compiler.context,
					llvm::APFloat(value));
		}
	};

	struct ASTVariable : public ASTNode
	{
		string id;
		Symbol* symbol;

		using ASTNode::getFrame;

		ASTVariable(const Position& position, const string& id):
			ASTNode(position),
			id(id), symbol(NULL)
		{
		}

		void resolveVariables()
		{
			SymbolTable& symbolTable = getFrame().getSymbolTable();
			symbol = symbolTable.resolve(id);
			if (!symbol)
				throw SymbolException(*this);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			return symbol->value(compiler.module);
		}
	};

	struct ASTVector : public ASTNode
	{
		ASTNode& x;
		ASTNode& y;
		ASTNode& z;

		ASTVector(const Position& position, ASTNode& x, ASTNode& y, ASTNode& z):
			ASTNode(position),
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

			v = compiler.builder.CreateInsertElement(v, xv, compiler.xindex);
			v = compiler.builder.CreateInsertElement(v, yv, compiler.yindex);
			v = compiler.builder.CreateInsertElement(v, zv, compiler.zindex);

			return v;
		}

		void resolveVariables()
		{
			x.resolveVariables();
			y.resolveVariables();
			z.resolveVariables();
		}
	};

	struct ASTExtract : public ASTNode
	{
		ASTNode& value;
		string id;

		ASTExtract(const Position& position, ASTNode& value, const string& id):
			ASTNode(position),
			value(value), id(id)
		{
			value.parent = this;
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = value.codegen_to_vector(compiler);

			llvm::Value* element = NULL;
			if (id == "x")
				element = compiler.xindex;
			else if (id == "y")
				element = compiler.yindex;
			else if (id == "z")
				element = compiler.zindex;
			assert(element);

			return compiler.builder.CreateExtractElement(v, element);
		}

		void resolveVariables()
		{
			value.resolveVariables();
		}
	};

	struct ASTFrame : public ASTNode
	{
		using ASTNode::parent;

		ASTFrame(const Position& position):
			ASTNode(position)
		{
		}

		ASTFrame& getFrame()
		{
			return *this;
		}

		virtual SymbolTable& getSymbolTable() = 0;

		ASTFrame& getParentFrame()
		{
			if (parent)
				return parent->getFrame();
			return *this;
		}
	};

	struct ASTDefineVariable : public ASTFrame
	{
		string id;
		char type;
		ASTNode& value;
		ASTNode& body;

		VariableSymbol _symbol;
		SingletonSymbolTable _symbolTable;
		using ASTFrame::getParentFrame;
		using ASTNode::parent;

		ASTDefineVariable(const Position& position,
				const string& id, char type,
				ASTNode& value, ASTNode& body):
			ASTFrame(position),
			id(id), value(value), body(body),
			_symbol(id),
			_symbolTable(getParentFrame().getSymbolTable(), _symbol)
		{
			body.parent = this;
		}

		void resolveVariables()
		{
			value.parent = parent;
			value.resolveVariables();
			body.resolveVariables();
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = value.codegen(compiler);
			_symbol.setValue(v);

			return body.codegen(compiler);
		}

		SymbolTable& getSymbolTable()
		{
			return _symbolTable;
		}
	};

	struct ASTFunction : public ASTFrame
	{
		ASTNode& definition;
		ASTNode& body;
		llvm::Function* _function;

		MultipleSymbolTable _symbolTable;
		using ASTFrame::getParentFrame;

		ASTFunction(const Position& position, ASTNode& definition, ASTNode& body):
			ASTFrame(position),
			definition(definition),
			body(body),
			_function(NULL),
			_symbolTable(getParentFrame().getSymbolTable())
		{
			definition.parent = body.parent = this;
		}

#if 0
		void visit(NodeVisitor& visitor)
		{
			ASTFrame::visit(visitor);
			visitor.visit(*this);
			definition.visit(visitor);
			body.visit(visitor);
		}
#endif

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

		SymbolTable& getSymbolTable()
		{
			return _symbolTable;
		}
	};

	struct ASTToplevel : public ASTFrame
	{
		ASTNode& body;

		SymbolTable& _symbolTable;
		using ASTFrame::getParentFrame;

		ASTToplevel(const Position& position, ASTNode& body,
				SymbolTable& symboltable):
			ASTFrame(position),
			body(body),
			_symbolTable(symboltable)
		{
			body.parent = this;
		}

		void resolveVariables()
		{
			body.resolveVariables();
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

		SymbolTable& getSymbolTable()
		{
			return _symbolTable;
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

	void expect_identifier(L& lexer, const string& s)
	{
		if ((lexer.token() != L::IDENTIFIER) || (lexer.id() != s))
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

	void parse_typesignature(L& lexer, vector<pair<string, char> >& arguments,
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
		parse_returntype(lexer, returntype);
	}

	void parse_returntype(L& lexer, char& returntype)
	{
		returntype = REAL;
		if (lexer.token() == L::COLON)
			parse_typespec(lexer, returntype);
	}

	ASTVariable& parse_variable(L& lexer)
	{
		Position position = lexer.position();

		string id;
		parse_identifier(lexer, id);
		return node(new ASTVariable(position, id));
	}

	ASTNode& parse_leaf(L& lexer)
	{
		switch (lexer.token())
		{
			case L::NUMBER:
			{
				Position position = lexer.position();
				Real value = lexer.real();
				lexer.next();
				return node(new ASTConstant(position, value));
			}

			case L::OPERATOR:
			case L::IDENTIFIER:
			{
				const string& id = lexer.id();
				if (id == "<")
					return parse_vector(lexer);
				else if (id == "let")
					return parse_let(lexer);
				return parse_variable(lexer);
			}
		}

		lexer.error("expected an expression");
		throw NULL; // do not return
	}

	ASTNode& parse_tight(L& lexer)
	{
		ASTNode& value = parse_leaf(lexer);

		if (lexer.token() == L::DOT)
		{
			Position position = lexer.position();
			expect(lexer, L::DOT);

			string id;
			parse_identifier(lexer, id);
			if ((id == "x") || (id == "y") || (id == "z"))
				return node(new ASTExtract(position, value, id));
		}

		return value;
	}

	ASTNode& parse_expression(L& lexer)
	{
		return parse_tight(lexer);
	}

	ASTFrame& parse_let(L& lexer)
	{
		Position position = lexer.position();

		expect_identifier(lexer, "let");

		string id;
		parse_identifier(lexer, id);

		char returntype;
		parse_returntype(lexer, returntype);

		expect_operator(lexer, "=");
		ASTNode& value = parse_expression(lexer);
		expect_operator(lexer, ";");
		ASTNode& body = parse_expression(lexer);
		return node(new ASTDefineVariable(position, id, returntype,
				value, body));
	}

	ASTVector& parse_vector(L& lexer)
	{
		Position position = lexer.position();

		expect_operator(lexer, "<");
		ASTNode& x = parse_leaf(lexer);
		expect(lexer, L::COMMA);
		ASTNode& y = parse_leaf(lexer);
		expect(lexer, L::COMMA);
		ASTNode& z = parse_leaf(lexer);
		expect_operator(lexer, ">");

		return node(new ASTVector(position, x, y, z));
	}

	ASTToplevel& parse_toplevel(L& lexer, SymbolTable& symboltable)
	{
		Position position = lexer.position();
		ASTNode& body = parse_tight(lexer);
		return node(new ASTToplevel(position, body, symboltable));
	}
};

#endif
