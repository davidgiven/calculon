#ifndef CALCULON_COMPILER_H
#define CALCULON_COMPILER_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class Compiler : public Allocator, public CompilerState
{
public:
	using CompilerState::builder;
	using CompilerState::module;
	using CompilerState::context;

private:
	class ASTNode;
	class ASTVariable;

	typedef Lexer<Real> L;
	typedef pair<string, char> Argument;

	using CompilerState::getInternalType;
	using CompilerState::intType;
	using CompilerState::xindex;
	using CompilerState::yindex;
	using CompilerState::zindex;
	using CompilerState::realType;
	using CompilerState::vectorType;
	using CompilerState::pointerType;

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
		CompilerState(context, module)
	{
		intType = llvm::IntegerType::get(context, 32);
		xindex = llvm::ConstantInt::get(intType, 0);
		yindex = llvm::ConstantInt::get(intType, 1);
		zindex = llvm::ConstantInt::get(intType, 2);
		realType = S::createRealType(context);
		vectorType = llvm::VectorType::get(realType, 4);

		llvm::Type* structType = llvm::StructType::get(
				realType, realType, realType, NULL);
		pointerType = llvm::PointerType::get(structType, 0);
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
	FunctionSymbol* compile(std::istream& signaturestream, std::istream& codestream,
			SymbolTable* globals)
	{
		vector<VariableSymbol*> arguments;
		char returntype;

		L signaturelexer(signaturestream);
		parse_functionsignature(signaturelexer, arguments, returntype);
		expect_eof(signaturelexer);

		FunctionSymbol* symbol = retain(new FunctionSymbol("<toplevel>",
				arguments, returntype));

		/* Create symbols and the LLVM type array for the function. */

		MultipleSymbolTable symboltable(globals);
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
		symbol->setFunction(f);

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
	#include "calculon_ast.h"

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
