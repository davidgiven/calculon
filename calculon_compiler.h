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
class Compiler : public CompilerBase<Real>, Allocator
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

	typedef Lexer<Real> L;
	typedef pair<string, char> Argument;

	enum
	{
		VECTOR = 'V'
	};
	using CompilerBase<Real>::REAL;
	using CompilerBase<Real>::createRealType;

	class TypeException : public CompilationException
	{
	public:
		TypeException(const string& what, ASTNode* node):
			CompilationException(node->position.formatError(what))
		{
		}
	};

	class SymbolException : public CompilationException
	{
		static string formatError(ASTVariable* node)
		{
			std::stringstream s;
			s << "unresolved symbol '" << node->id << "'";
			return node->position.formatError(s.str());
		}

	public:
		SymbolException(ASTVariable* node):
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

private:
	llvm::Type* get_type_from_char(char c)
	{
		switch (c)
		{
			case REAL: return realType;
			case VECTOR: return vectorType;
		}
		assert(false);
	}

public:
	void compile(std::istream& signaturestream, std::istream& codestream)
	{
		vector<VariableSymbol*> arguments;
		char returntype;

		L signaturelexer(signaturestream);
		parse_functionsignature(signaturelexer, arguments, returntype);
		expect_eof(signaturelexer);

		/* Create symbols and the LLVM type array for the function. */

		MultipleSymbolTable symboltable;
		vector<llvm::Type*> llvmtypes;
		for (int i=0; i<arguments.size(); i++)
		{
			VariableSymbol* symbol = arguments[i];
			symboltable.add(symbol);
			llvmtypes.push_back(get_type_from_char(symbol->type()));
		}

		/* Compile the code to an AST. */

		L codelexer(codestream);
		ASTToplevel* ast = parse_toplevel(codelexer, symboltable);
		ast->resolveVariables(*this);

		/* Create the LLVM function itself. */

		llvm::FunctionType* ft = llvm::FunctionType::get(
				get_type_from_char(returntype), llvmtypes, false);

		llvm::Function* f = llvm::Function::Create(ft,
				llvm::Function::InternalLinkage,
				"toplevel", module);

		/* Bind the argument symbols to their LLVM values. */

		{
			int i = 0;
			for (llvm::Function::arg_iterator ii = f->arg_begin(),
					ee = f->arg_end(); ii != ee; ii++)
			{
				llvm::Value* v = ii;
				VariableSymbol* symbol = arguments[i];

				v->setName(symbol->name());
				symbol->setValue(v);
				i++;
			}
		}

		/* Generate the IR code. */

		llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "", f);
		builder.SetInsertPoint(bb);
		ast->codegen(*this);
	}

private:
	class ASTFrame;
	class ASTFunction;

	struct ASTNode : public Object
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
				throw TypeException("type mismatch: expected a real", this);
			return v;
		}

		llvm::Value* codegen_to_vector(Compiler& compiler)
		{
			llvm::Value* v = codegen(compiler);

			if (v->getType() != compiler.vectorType)
				throw TypeException("type mismatch: expected a vector", this);
			return v;
		}

		virtual void resolveVariables(Compiler& compiler)
		{
		}

		virtual ASTFrame* getFrame()
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

		void resolveVariables(Compiler& compiler)
		{
			SymbolTable& symbolTable = getFrame()->getSymbolTable();
			symbol = symbolTable.resolve(id);
			if (!symbol)
				throw SymbolException(this);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			return symbol->value(compiler.module);
		}
	};

	struct ASTVector : public ASTNode
	{
		ASTNode* x;
		ASTNode* y;
		ASTNode* z;

		ASTVector(const Position& position, ASTNode* x, ASTNode* y, ASTNode* z):
			ASTNode(position),
			x(x), y(y), z(z)
		{
			x->parent = y->parent = z->parent = this;
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = llvm::UndefValue::get(compiler.vectorType);

			llvm::Value* xv = x->codegen_to_real(compiler);
			llvm::Value* yv = y->codegen_to_real(compiler);
			llvm::Value* zv = z->codegen_to_real(compiler);

			v = compiler.builder.CreateInsertElement(v, xv, compiler.xindex);
			v = compiler.builder.CreateInsertElement(v, yv, compiler.yindex);
			v = compiler.builder.CreateInsertElement(v, zv, compiler.zindex);

			return v;
		}

		void resolveVariables(Compiler& compiler)
		{
			x->resolveVariables(compiler);
			y->resolveVariables(compiler);
			z->resolveVariables(compiler);
		}
	};

	struct ASTExtract : public ASTNode
	{
		ASTNode* value;
		string id;

		ASTExtract(const Position& position, ASTNode* value, const string& id):
			ASTNode(position),
			value(value), id(id)
		{
			value->parent = this;
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = value->codegen_to_vector(compiler);

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

		void resolveVariables(Compiler& compiler)
		{
			value->resolveVariables(compiler);
		}
	};

	struct ASTFrame : public ASTNode
	{
		using ASTNode::parent;

		ASTFrame(const Position& position):
			ASTNode(position)
		{
		}

		ASTFrame* getFrame()
		{
			return this;
		}

		virtual SymbolTable& getSymbolTable() = 0;

		ASTFrame* getParentFrame()
		{
			if (parent)
				return parent->getFrame();
			return this;
		}
	};

	struct ASTDefineVariable : public ASTFrame
	{
		string id;
		char type;
		ASTNode* value;
		ASTNode* body;

		SingletonSymbolTable _symbolTable;
		VariableSymbol* _symbol;
		using ASTFrame::getParentFrame;
		using ASTNode::parent;

		ASTDefineVariable(const Position& position,
				const string& id, char type,
				ASTNode* value, ASTNode* body):
			ASTFrame(position),
			id(id), value(value), body(body),
			_symbolTable(getParentFrame()->getSymbolTable()),
			_symbol(NULL)
		{
			body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			_symbol = compiler.retain(new VariableSymbol(id, type));
			_symbolTable.add(_symbol);

			value->parent = parent;
			value->resolveVariables(compiler);
			body->resolveVariables(compiler);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = value->codegen(compiler);
			_symbol->setValue(v);

			return body->codegen(compiler);
		}

		SymbolTable& getSymbolTable()
		{
			return _symbolTable;
		}
	};

	struct ASTFunctionBody : public ASTFrame
	{
		ASTNode* body;

		MultipleSymbolTable _symbolTable;
		using ASTFrame::getParentFrame;

		ASTFunctionBody(const Position& position, ASTNode* body):
			ASTFrame(position),
			body(body),
			_symbolTable(getParentFrame()->getSymbolTable())
		{
			body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			body->resolveVariables();
		}
	};

	struct ASTDefineFunction : public ASTFrame
	{
		ASTNode* definition;
		ASTNode* body;
		llvm::Function* _function;

		SingletonSymbolTable _symbolTable;
		using ASTFrame::getParentFrame;

		ASTDefineFunction(const Position& position, ASTNode* definition, ASTNode* body):
			ASTFrame(position),
			definition(definition),
			body(body),
			_function(NULL),
			_symbolTable(getParentFrame()->getSymbolTable())
		{
			definition->parent = body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			definition->resolveVariables();
			body->resolveVariables();
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			/* Compile the function definition itself. */

			{
				llvm::BasicBlock* bb = compiler.builder.GetInsertBlock();
				llvm::BasicBlock::iterator bi = compiler.builder.GetInsertPoint();

#if 0
#endif
#if 0
				llvm::FunctionType* ft = llvm::FunctionType::get(
						compiler.get_type_from_char(returntype), llvmtypes, false);

				llvm::Function* f = llvm::Function::Create(ft,
						llvm::Function::InternalLinkage,
						"", compiler.module);

				llvm::BasicBlock* toplevel = llvm::BasicBlock::Create(
						compiler, "", f);
				compiler.SetInsertPoint(toplevel);
#endif

				llvm::Value* v = definition->codegen(compiler);
				compiler.builder.CreateRet(v);

				compiler.builder.SetInsertPoint(bb, bi);
			}

			return body->codegen(compiler);
		}

		SymbolTable& getSymbolTable()
		{
			return _symbolTable;
		}
	};

	struct ASTToplevel : public ASTFrame
	{
		ASTNode* body;

		SymbolTable& _symbolTable;
		using ASTFrame::getParentFrame;

		ASTToplevel(const Position& position, ASTNode* body,
				SymbolTable& symboltable):
			ASTFrame(position),
			body(body),
			_symbolTable(symboltable)
		{
			body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			body->resolveVariables(compiler);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = body->codegen(compiler);
			compiler.builder.CreateRet(v);
			return NULL;
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
		if (lexer.token() != L::COLON)
			type = REAL;
		else
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
	}

	void parse_functionsignature(L& lexer, vector<VariableSymbol*>& arguments,
			char& returntype)
	{
		expect(lexer, L::OPENPAREN);

		while (lexer.token() != L::CLOSEPAREN)
		{
			string id;
			char type = REAL;

			parse_identifier(lexer, id);

			if (lexer.token() == L::COLON)
				parse_typespec(lexer, type);

			VariableSymbol* symbol = retain(new VariableSymbol(id, type));
			arguments.push_back(symbol);
			parse_list_separator(lexer);
		}

		expect(lexer, L::CLOSEPAREN);
		parse_typespec(lexer, returntype);
	}

	ASTVariable* parse_variable(L& lexer)
	{
		Position position = lexer.position();

		string id;
		parse_identifier(lexer, id);
		return retain(new ASTVariable(position, id));
	}

	ASTNode* parse_leaf(L& lexer)
	{
		switch (lexer.token())
		{
			case L::NUMBER:
			{
				Position position = lexer.position();
				Real value = lexer.real();
				lexer.next();
				return retain(new ASTConstant(position, value));
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
		throw (void*) NULL; // do not return
	}

	ASTNode* parse_tight(L& lexer)
	{
		ASTNode* value = parse_leaf(lexer);

		if (lexer.token() == L::DOT)
		{
			Position position = lexer.position();
			expect(lexer, L::DOT);

			string id;
			parse_identifier(lexer, id);
			if ((id == "x") || (id == "y") || (id == "z"))
				return retain(new ASTExtract(position, value, id));
		}

		return value;
	}

	ASTNode* parse_expression(L& lexer)
	{
		return parse_tight(lexer);
	}

	ASTFrame* parse_let(L& lexer)
	{
		Position position = lexer.position();

		expect_identifier(lexer, "let");

		string id;
		parse_identifier(lexer, id);

		char returntype;
		parse_typespec(lexer, returntype);

		expect_operator(lexer, "=");
		ASTNode* value = parse_expression(lexer);
		expect_operator(lexer, ";");
		ASTNode* body = parse_expression(lexer);
		return retain(new ASTDefineVariable(position, id, returntype,
				value, body));
	}

	ASTVector* parse_vector(L& lexer)
	{
		Position position = lexer.position();

		expect_operator(lexer, "<");
		ASTNode* x = parse_expression(lexer);
		expect(lexer, L::COMMA);
		ASTNode* y = parse_expression(lexer);
		expect(lexer, L::COMMA);
		ASTNode* z = parse_expression(lexer);
		expect_operator(lexer, ">");

		return retain(new ASTVector(position, x, y, z));
	}

	ASTToplevel* parse_toplevel(L& lexer, SymbolTable& symboltable)
	{
		Position position = lexer.position();
		ASTNode* body = parse_tight(lexer);
		return retain(new ASTToplevel(position, body, symboltable));
	}
};

#endif
