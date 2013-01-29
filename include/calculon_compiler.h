/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_COMPILER_H
#define CALCULON_COMPILER_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class Compiler : public CompilerState
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

	using CompilerState::retain;
	using CompilerState::getInternalType;
	using CompilerState::intType;
	using CompilerState::xindex;
	using CompilerState::yindex;
	using CompilerState::zindex;
	using CompilerState::realType;
	using CompilerState::doubleType;
	using CompilerState::floatType;
	using CompilerState::vectorType;
	using CompilerState::pointerType;
	using CompilerState::booleanType;

	map<string, int> _operatorPrecedence;

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
	Compiler(llvm::LLVMContext& context, llvm::Module* module,
			llvm::ExecutionEngine* engine):
		CompilerState(context, module, engine)
	{
		intType = llvm::IntegerType::get(context, 32);
		xindex = llvm::ConstantInt::get(intType, 0);
		yindex = llvm::ConstantInt::get(intType, 1);
		zindex = llvm::ConstantInt::get(intType, 2);
		realType = S::createRealType(context);
		doubleType = llvm::Type::getDoubleTy(context);
		floatType = llvm::Type::getFloatTy(context);
		vectorType = llvm::VectorType::get(realType, 4);
		booleanType = llvm::IntegerType::get(context, 1);

		llvm::Type* structType = llvm::StructType::get(
				realType, realType, realType, NULL);
		pointerType = llvm::PointerType::get(structType, 0);

		_operatorPrecedence["and"] = 5;
		_operatorPrecedence["or"] = 5;
		_operatorPrecedence["<"] = 10;
		_operatorPrecedence["<="] = 10;
		_operatorPrecedence[">"] = 10;
		_operatorPrecedence[">="] = 10;
		_operatorPrecedence["=="] = 10;
		_operatorPrecedence["!="] = 10;
		_operatorPrecedence["+"] = 20;
		_operatorPrecedence["-"] = 20;
		_operatorPrecedence["*"] = 30;
		_operatorPrecedence["/"] = 30;
	}

	char llvmToType(llvm::Type* t)
	{
		if (t == realType)
			return REAL;
		else if (t == vectorType)
			return VECTOR;
		else if (t == booleanType)
			return BOOLEAN;
		assert(false);
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

		FunctionSymbol* functionsymbol = retain(new FunctionSymbol("<toplevel>",
				arguments, returntype));

		/* Create symbols and the LLVM type array for the function. */

		MultipleSymbolTable symboltable(globals);
		vector<llvm::Type*> llvmtypes;
		for (int i=0; i<arguments.size(); i++)
		{
			VariableSymbol* symbol = arguments[i];
			symbol->function = functionsymbol;
			symboltable.add(symbol);
			llvmtypes.push_back(getInternalType(symbol->type));
		}

		/* Compile the code to an AST. */

		L codelexer(codestream);
		ASTToplevel* ast = parse_toplevel(codelexer, functionsymbol, &symboltable);
		ast->resolveVariables(*this);

		/* Create the LLVM function itself. */

		llvm::FunctionType* ft = llvm::FunctionType::get(
				getInternalType(returntype), llvmtypes, false);

		llvm::Function* f = llvm::Function::Create(ft,
				llvm::Function::InternalLinkage,
				"toplevel", module);
		functionsymbol->function = f;

		/* Bind the argument symbols to their LLVM values. */

		{
			int i = 0;
			for (llvm::Function::arg_iterator ii = f->arg_begin(),
					ee = f->arg_end(); ii != ee; ii++)
			{
				llvm::Value* v = ii;
				VariableSymbol* symbol = arguments[i];

				v->setName(symbol->name);
				symbol->value = v;
				i++;
			}
		}

		/* Generate the IR code. */

		llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "", f);
		builder.SetInsertPoint(bb);
		ast->codegen(*this);

		return functionsymbol;
	}

private:
	#include "calculon_ast.h"

private:
	void expect(L& lexer, int token)
	{
		if (lexer.token() != token)
		{
			std::stringstream s;
			s << "expected " << lexer.tokenname(token);
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
			else if (lexer.id() == "boolean")
				type = BOOLEAN;
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

			if ((id == "true") || (id == "false"))
				return retain(new ASTBoolean(position, id));
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

			case L::OPENBLOCK:
				return parse_vector(lexer);

			case L::OPERATOR:
			case L::IDENTIFIER:
			{
				const string& id = lexer.id();
				if (id == "let")
					return parse_let(lexer);
				else if (id == "if")
					return parse_if(lexer);
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

			vector<ASTNode*> parameters;
			parameters.push_back(value);
			return retain(new ASTFunctionCall(position, "method "+id,
					parameters));
		}

		return value;
	}

	ASTNode* parse_unary(L& lexer)
	{
		if (lexer.token() == L::OPERATOR)
		{
			Position position = lexer.position();
			string id = lexer.id();

			if ((id == "-") || (id == "not"))
			{
				lexer.next();

				ASTNode* value = parse_tight(lexer);
				vector<ASTNode*> parameters;
				parameters.push_back(value);
				return retain(new ASTFunctionCall(position, "method "+id,
						parameters));
			}
		}

		return parse_tight(lexer);
	}

	ASTNode* parse_binary(L& lexer, int precedence)
	{
		ASTNode* lhs = parse_unary(lexer);

		while (lexer.token() == L::OPERATOR)
		{
			Position position = lexer.position();
			string id = lexer.id();
			int p = _operatorPrecedence[id];
			if (p == 0)
				lexer.error("unrecognised operator");

			if (p < precedence)
				break;

			lexer.next();
			ASTNode* rhs = parse_binary(lexer, p+1);

			if (id == "and")
				lhs = retain(new ASTCondition(position, lhs,
						rhs, retain(new ASTBoolean(position, "false"))));
			else if (id == "or")
				lhs = retain(new ASTCondition(position, lhs,
						retain(new ASTBoolean(position, "true")), rhs));
			else
			{
				vector<ASTNode*> parameters;
				parameters.push_back(lhs);
				parameters.push_back(rhs);
				lhs = retain(new ASTFunctionCall(position, "method "+id,
						parameters));
			}
		}

		return lhs;
	}

	ASTNode* parse_expression(L& lexer)
	{
		return parse_binary(lexer, 0);
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
			expect(lexer, L::SEMICOLON);
			ASTNode* body = parse_expression(lexer);
			return retain(new ASTDefineFunction(position, f, definition, body));
		}
		else
		{
			/* Variable definition. */

			expect_operator(lexer, "=");
			ASTNode* value = parse_expression(lexer);
			expect(lexer, L::SEMICOLON);
			ASTNode* body = parse_expression(lexer);
			return retain(new ASTDefineVariable(position, id, returntype,
					value, body));
		}
	}

	ASTNode* parse_if(L& lexer)
	{
		Position position = lexer.position();

		expect_identifier(lexer, "if");
		ASTNode* condition = parse_expression(lexer);
		expect_identifier(lexer, "then");
		ASTNode* trueval = parse_expression(lexer);
		expect_identifier(lexer, "else");
		ASTNode* falseval = parse_expression(lexer);

		return retain(new ASTCondition(position, condition, trueval, falseval));
	}

	ASTVector* parse_vector(L& lexer)
	{
		Position position = lexer.position();

		expect(lexer, L::OPENBLOCK);
		ASTNode* x = parse_expression(lexer);
		expect(lexer, L::COMMA);
		ASTNode* y = parse_expression(lexer);
		expect(lexer, L::COMMA);
		ASTNode* z = parse_expression(lexer);
		expect(lexer, L::CLOSEBLOCK);

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
