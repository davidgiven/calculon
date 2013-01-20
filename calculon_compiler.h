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
public:
	enum
	{
		REAL = 'D'
	};

protected:
	static llvm::Type* createRealType(llvm::LLVMContext& context)
	{
		return llvm::Type::getDoubleTy(context);
	}
};

template <>
class CompilerBase<float>
{
public:
	enum
	{
		REAL = 'F'
	};

protected:
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
	llvm::Type* pointerType;

	using CompilerBase<Real>::REAL;
	enum
	{
		VECTOR = 'V'
	};

private:
	class ASTNode;
	class ASTVariable;

	typedef Lexer<Real> L;
	typedef pair<string, char> Argument;

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
		static string formatError(const string& id, ASTNode* node)
		{
			std::stringstream s;
			s << "unresolved symbol '" << id << "'";
			return node->position.formatError(s.str());
		}

	public:
		SymbolException(const string& id, ASTNode* node):
			CompilationException(formatError(id, node))
		{
		}
	};

	class OperatorException : public CompilationException
	{
		static string formatError(const string& id, ASTNode* node)
		{
			std::stringstream s;
			s << "unknown operator '" << id << "'";
			return node->position.formatError(s.str());
		}

	public:
		OperatorException(const string& id, ASTNode* node):
			CompilationException(formatError(id, node))
		{
		}
	};

	class IntrinsicTypeException : public CompilationException
	{
		static string formatError(const string& id, char type, ASTNode* node)
		{
			std::stringstream s;
			s << "wrong type applied to '" << id << "'";
			return node->position.formatError(s.str());
		}

	public:
		IntrinsicTypeException(const string& id, char type, ASTNode* node):
			CompilationException(formatError(id, type, node))
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

		llvm::Type* structType = llvm::StructType::get(
				realType, realType, realType, NULL);
		pointerType = llvm::PointerType::get(structType, 0);
	}

	llvm::Type* getInternalType(char c)
	{
		switch (c)
		{
			case REAL: return realType;
			case VECTOR: return vectorType;
		}
		assert(false);
	}

	llvm::Type* getExternalType(char c)
	{
		switch (c)
		{
			case REAL: return realType;
			case VECTOR: return pointerType;
		}
		assert(false);
	}

	char llvmToType(llvm::Type* t)
	{
		if (t == realType)
			return REAL;
		else if (t == vectorType)
			return VECTOR;
		assert(false);
	}

	void storeVector(llvm::Value* v, llvm::Value* p)
	{
		llvm::Value* xv = builder.CreateExtractElement(v, xindex);
		llvm::Value* yv = builder.CreateExtractElement(v, yindex);
		llvm::Value* zv = builder.CreateExtractElement(v, zindex);

		builder.CreateStore(xv, builder.CreateStructGEP(p, 0));
		builder.CreateStore(yv, builder.CreateStructGEP(p, 1));
		builder.CreateStore(zv, builder.CreateStructGEP(p, 2));
	}

	llvm::Value* loadVector(llvm::Value* p)
	{
		llvm::Value* xv = builder.CreateLoad(builder.CreateStructGEP(p, 0));
		llvm::Value* yv = builder.CreateLoad(builder.CreateStructGEP(p, 1));
		llvm::Value* zv = builder.CreateLoad(builder.CreateStructGEP(p, 2));

		llvm::Value* v = llvm::UndefValue::get(vectorType);
		v = builder.CreateInsertElement(v, xv, xindex);
		v = builder.CreateInsertElement(v, yv, yindex);
		v = builder.CreateInsertElement(v, zv, zindex);
		return v;
	}

public:
	FunctionSymbol* compile(std::istream& signaturestream, std::istream& codestream)
	{
		vector<VariableSymbol*> arguments;
		char returntype;

		L signaturelexer(signaturestream);
		parse_functionsignature(signaturelexer, arguments, returntype);
		expect_eof(signaturelexer);

		FunctionSymbol* symbol = retain(new FunctionSymbol("<toplevel>",
				arguments, returntype));

		/* Create symbols and the LLVM type array for the function. */

		MultipleSymbolTable symboltable;
		vector<llvm::Type*> llvmtypes;
		for (int i=0; i<arguments.size(); i++)
		{
			VariableSymbol* symbol = arguments[i];
			symboltable.add(symbol);
			llvmtypes.push_back(getInternalType(symbol->type()));
		}

		/* Compile the code to an AST. */

		L codelexer(codestream);
		ASTToplevel* ast = parse_toplevel(codelexer, symbol, &symboltable);
		ast->resolveVariables(*this);

		/* Create the LLVM function itself. */

		llvm::FunctionType* ft = llvm::FunctionType::get(
				getInternalType(returntype), llvmtypes, false);

		llvm::Function* f = llvm::Function::Create(ft,
				llvm::Function::InternalLinkage,
				"toplevel", module);
		symbol->setValue(f);

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

		return symbol;
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
			SymbolTable* symbolTable = getFrame()->symbolTable;
			symbol = symbolTable->resolve(id);
			if (!symbol)
				throw SymbolException(id, this);
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

	struct ASTUnary : public ASTNode
	{
		string id;
		ASTNode* value;

		ASTUnary(const Position& position, const string& id, ASTNode* value):
			ASTNode(position),
			id(id), value(value)
		{
			value->parent = this;
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			llvm::Value* v = value->codegen(compiler);
			char type = compiler.llvmToType(v->getType());

			if (id == "-")
			{
				switch (type)
				{
					case REAL:
					case VECTOR:
						v = compiler.builder.CreateFNeg(v);
						break;

					default:
						throw IntrinsicTypeException(id, type, this);
				}
			}
			else
				throw SymbolException(id, this);

			return v;
		}

		void resolveVariables(Compiler& compiler)
		{
			value->resolveVariables(compiler);
		}
	};

	struct ASTFrame : public ASTNode
	{
		SymbolTable* symbolTable;

		using ASTNode::parent;

		ASTFrame(const Position& position):
			ASTNode(position),
			symbolTable(NULL)
		{
		}

		ASTFrame* getFrame()
		{
			return this;
		}
	};

	struct ASTDefineVariable : public ASTFrame
	{
		string id;
		char type;
		ASTNode* value;
		ASTNode* body;

		VariableSymbol* _symbol;
		using ASTFrame::symbolTable;
		using ASTNode::parent;

		ASTDefineVariable(const Position& position,
				const string& id, char type,
				ASTNode* value, ASTNode* body):
			ASTFrame(position),
			id(id), value(value), body(body),
			_symbol(NULL)
		{
			body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			symbolTable = compiler.retain(new SingletonSymbolTable(
					parent->getFrame()->symbolTable));
			_symbol = compiler.retain(new VariableSymbol(id, type));
			symbolTable->add(_symbol);

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
	};

	struct ASTFunctionBody : public ASTFrame
	{
		FunctionSymbol* function;
		ASTNode* body;

		using ASTNode::parent;
		using ASTFrame::symbolTable;

		ASTFunctionBody(const Position& position,
				FunctionSymbol* function, ASTNode* body):
			ASTFrame(position),
			function(function), body(body)
		{
			body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			if (!symbolTable)
				symbolTable = compiler.retain(new MultipleSymbolTable(
					parent->getFrame()->symbolTable));

			vector<VariableSymbol*>& arguments = function->arguments();
			for (vector<VariableSymbol*>::const_iterator i = arguments.begin(),
					e = arguments.end(); i != e; i++)
			{
				symbolTable->add(*i);
			}

			body->resolveVariables(compiler);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			/* Create the LLVM function type. */

			vector<VariableSymbol*>& arguments = function->arguments();
			vector<llvm::Type*> llvmtypes;
			for (int i=0; i<arguments.size(); i++)
			{
				VariableSymbol* symbol = arguments[i];
				llvmtypes.push_back(compiler.getInternalType(symbol->type()));
			}

			llvm::Type* returntype = compiler.getInternalType(function->returntype());
			llvm::FunctionType* ft = llvm::FunctionType::get(
					returntype, llvmtypes, false);

			/* Create the function. */

			llvm::Function* f = llvm::Function::Create(ft,
					llvm::Function::InternalLinkage,
					function->name(), compiler.module);
			function->setValue(f);

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

			/* Generate the code. */

			llvm::BasicBlock* toplevel = llvm::BasicBlock::Create(
					compiler.context, "", f);

			llvm::BasicBlock* bb = compiler.builder.GetInsertBlock();
			llvm::BasicBlock::iterator bi = compiler.builder.GetInsertPoint();
			compiler.builder.SetInsertPoint(toplevel);

			llvm::Value* v = body->codegen(compiler);
			compiler.builder.CreateRet(v);
			if (v->getType() != returntype)
				throw TypeException(
						"function does not return the type it's declared to return", this);

			compiler.builder.SetInsertPoint(bb, bi);

			return f;
		}
	};

	struct ASTDefineFunction : public ASTFrame
	{
		FunctionSymbol* function;
		ASTNode* definition;
		ASTNode* body;

		using ASTNode::parent;
		using ASTFrame::symbolTable;

		ASTDefineFunction(const Position& position, FunctionSymbol* function,
				ASTNode* definition, ASTNode* body):
			ASTFrame(position),
			function(function), definition(definition), body(body)
		{
			definition->parent = body->parent = this;
		}

		void resolveVariables(Compiler& compiler)
		{
			symbolTable = compiler.retain(new SingletonSymbolTable(
					parent->getFrame()->symbolTable));
			symbolTable->add(function);

			definition->resolveVariables(compiler);
			body->resolveVariables(compiler);
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			definition->codegen(compiler);
			return body->codegen(compiler);
		}
	};

	struct ASTFunctionCall : public ASTNode
	{
		string id;
		vector<ASTNode*> arguments;
		FunctionSymbol* function;

		using ASTNode::position;
		using ASTNode::getFrame;

		ASTFunctionCall(const Position& position, const string& id,
				const vector<ASTNode*>& arguments):
			ASTNode(position),
			id(id), arguments(arguments),
			function(NULL)
		{
			for (typename vector<ASTNode*>::const_iterator i = arguments.begin(),
					e = arguments.end(); i != e; i++)
			{
				(*i)->parent = this;
			}
		}

		void resolveVariables(Compiler& compiler)
		{
			Symbol* symbol = getFrame()->symbolTable->resolve(id);
			if (!symbol)
				throw SymbolException(id, this);
			if (!symbol->isFunction())
			{
				std::stringstream s;
				s << "attempt to call '" << id << "', which is not a function";
				throw CompilationException(position.formatError(s.str()));
			}
			function = (FunctionSymbol*) symbol;

			for (typename vector<ASTNode*>::const_iterator i = arguments.begin(),
					e = arguments.end(); i != e; i++)
			{
				(*i)->resolveVariables(compiler);
			}
		}

		llvm::Value* codegen(Compiler& compiler)
		{
			vector<llvm::Value*> args;

			if (arguments.size() != function->arguments().size())
			{
				std::stringstream s;
				s << "attempt to call function '" << id <<
						"' with the wrong number of parameters";
				throw CompilationException(position.formatError(s.str()));
			}

			int i = 1;
			typename vector<ASTNode*>::const_iterator argi = arguments.begin();
			vector<VariableSymbol*>::const_iterator parami = function->arguments().begin();
			while (argi != arguments.end())
			{
				llvm::Value* v = (*argi)->codegen(compiler);
				if (compiler.llvmToType(v->getType()) != (*parami)->type())
				{
					std::stringstream s;
					s << "call to parameter " << i << " of function '" << id
							<< "' with wrong type";
					throw CompilationException(position.formatError(s.str()));
				}

				args.push_back(v);
				i++;
				argi++;
				parami++;
			}

			llvm::Value* f = function->value(compiler.module);
			assert(f);
			return compiler.builder.CreateCall(f, args);
		}
	};

	struct ASTToplevel : public ASTFunctionBody
	{
		using ASTFrame::symbolTable;

		ASTToplevel(const Position& position, FunctionSymbol* symbol,
				ASTNode* body, SymbolTable* st):
			ASTFunctionBody(position, symbol, body)
		{
			symbolTable = st;
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
		if (lexer.token() != L::ENDOFFILE)
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

	ASTNode* parse_variable_or_function_call(L& lexer)
	{
		Position position = lexer.position();

		string id;
		parse_identifier(lexer, id);

		if (lexer.token() == L::OPENPAREN)
		{
			/* Function call. */
			expect(lexer, L::OPENPAREN);

			vector<ASTNode*> arguments;
			while (lexer.token() != L::CLOSEPAREN)
			{
				arguments.push_back(parse_expression(lexer));
				parse_list_separator(lexer);
			}

			expect(lexer, L::CLOSEPAREN);

			return retain(new ASTFunctionCall(position, id, arguments));
		}
		else
		{
			/* Variable reference. */
			return retain(new ASTVariable(position, id));
		}
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

			case L::OPENPAREN:
			{
				expect(lexer, L::OPENPAREN);
				ASTNode* v = parse_expression(lexer);
				expect(lexer, L::CLOSEPAREN);
				return v;
			}

			case L::OPERATOR:
			case L::IDENTIFIER:
			{
				const string& id = lexer.id();
				if (id == "<")
					return parse_vector(lexer);
				else if (id == "let")
					return parse_let(lexer);
				return parse_variable_or_function_call(lexer);
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

	ASTNode* parse_unary(L& lexer)
	{
		if (lexer.token() == L::OPERATOR)
		{
			Position position = lexer.position();
			string id = lexer.id();

			if (id == "-")
			{
				lexer.next();

				ASTNode* value = parse_tight(lexer);
				return retain(new ASTUnary(position, id, value));
			}
		}

		return parse_tight(lexer);
	}

	ASTNode* parse_expression(L& lexer)
	{
		return parse_unary(lexer);
	}

	ASTFrame* parse_let(L& lexer)
	{
		Position position = lexer.position();

		expect_identifier(lexer, "let");

		string id;
		parse_identifier(lexer, id);

		char returntype;
		parse_typespec(lexer, returntype);

		if (lexer.token() == L::OPENPAREN)
		{
			/* Function definition. */

			vector<VariableSymbol*> arguments;
			char returntype;
			parse_functionsignature(lexer, arguments, returntype);

			FunctionSymbol* f = retain(
					new FunctionSymbol(id, arguments, returntype));

			expect_operator(lexer, "=");
			ASTNode* value = parse_expression(lexer);
			ASTFunctionBody* definition = retain(
					new ASTFunctionBody(position, f, value));
			expect_operator(lexer, ";");
			ASTNode* body = parse_expression(lexer);
			return retain(new ASTDefineFunction(position, f, definition, body));
		}
		else
		{
			/* Variable definition. */

			expect_operator(lexer, "=");
			ASTNode* value = parse_expression(lexer);
			expect_operator(lexer, ";");
			ASTNode* body = parse_expression(lexer);
			return retain(new ASTDefineVariable(position, id, returntype,
					value, body));
		}
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

	ASTToplevel* parse_toplevel(L& lexer, FunctionSymbol* symbol,
			SymbolTable* symboltable)
	{
		Position position = lexer.position();
		ASTNode* body = parse_expression(lexer);
		return retain(new ASTToplevel(position, symbol, body, symboltable));
	}
};

#endif
