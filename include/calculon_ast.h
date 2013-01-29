/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_AST_H
#define CALCULON_AST_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

class ASTFrame;

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

	llvm::Value* codegen_to_boolean(Compiler& compiler)
	{
		llvm::Value* v = codegen(compiler);

		if (v->getType() != compiler.booleanType)
			throw TypeException("type mismatch: expected a boolean", this);
		return v;
	}

	virtual void resolveVariables(Compiler& compiler)
	{
	}

	virtual ASTFrame* getFrame()
	{
		return parent->getFrame();
	}

	virtual FunctionSymbol* getFunction()
	{
		return parent->getFunction();
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

struct ASTBoolean : public ASTNode
{
	string id;

	ASTBoolean(const Position& position, const string& id):
		ASTNode(position),
		id(id)
	{
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		if (id == "true")
			return llvm::ConstantInt::getTrue(compiler.booleanType);
		else
			return llvm::ConstantInt::getFalse(compiler.booleanType);
	}
};

struct ASTVariable : public ASTNode
{
	string id;
	ValuedSymbol* symbol;

	using ASTNode::getFrame;
	using ASTNode::getFunction;
	using ASTNode::position;

	ASTVariable(const Position& position, const string& id):
		ASTNode(position),
		id(id), symbol(NULL)
	{
	}

	void resolveVariables(Compiler& compiler)
	{
		SymbolTable* symbolTable = getFrame()->symbolTable;
		Symbol* s = symbolTable->resolve(id);
		if (!s)
			throw SymbolException(id, this);
		symbol = s->isValued();
		if (!symbol)
		{
			std::stringstream s;
			s << "attempt to get the value of '" << id << "', which is not a variable";
			throw CompilationException(position.formatError(s.str()));
		}

		VariableSymbol* v = symbol->isVariable();
		if (v)
			symbol = getFunction()->importUpvalue(compiler, v);
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		return symbol->value;
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
	using ASTNode::getFunction;

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
		_symbol->function = getFunction();
		symbolTable->add(_symbol);

		_symbol->function->locals[_symbol] = _symbol;

		value->parent = parent;
		value->resolveVariables(compiler);
		body->resolveVariables(compiler);
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		llvm::Value* v = value->codegen(compiler);
		_symbol->value = v;

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

	FunctionSymbol* getFunction()
	{
		return function;
	}

	void resolveVariables(Compiler& compiler)
	{
		if (!symbolTable)
			symbolTable = compiler.retain(new MultipleSymbolTable(
				parent->getFrame()->symbolTable));

		const vector<VariableSymbol*>& arguments = function->arguments;
		for (typename vector<VariableSymbol*>::const_iterator i = arguments.begin(),
				e = arguments.end(); i != e; i++)
		{
			VariableSymbol* symbol = *i;
			symbol->function = function;
			symbolTable->add(symbol);
		}

		body->resolveVariables(compiler);
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		/* Assemble the LLVM function type. */

		const vector<VariableSymbol*>& arguments = function->arguments;
		vector<llvm::Type*> llvmtypes;

		/* Normal parameters... */

		for (typename vector<VariableSymbol*>::const_iterator i = arguments.begin(),
				e = arguments.end(); i != e; i++)
		{
			llvmtypes.push_back(compiler.getInternalType((*i)->type));
		}

		/* ...and imported upvalues. */

		for (typename FunctionSymbol::LocalsMap::const_iterator i = function->locals.begin(),
				e = function->locals.end(); i != e; i++)
		{
			if (i->first != i->second)
			{
				VariableSymbol* symbol = i->first; // root variable!
				assert(symbol->value);
				llvmtypes.push_back(symbol->value->getType());
			}
		}

		llvm::Type* returntype = compiler.getInternalType(function->returntype);
		llvm::FunctionType* ft = llvm::FunctionType::get(
				returntype, llvmtypes, false);

		/* Create the function. */

		llvm::Function* f = llvm::Function::Create(ft,
				llvm::Function::InternalLinkage,
				function->name, compiler.module);
		function->function = f;

		/* Bind the argument symbols to their LLVM values. */

		{
			llvm::Function::arg_iterator vi = f->arg_begin();

			/* First, normal parameters. */

			typename vector<VariableSymbol*>::const_iterator ai = arguments.begin();
			while (ai != arguments.end())
			{
				VariableSymbol* symbol = *ai;
				assert(vi != f->arg_end());
				vi->setName(symbol->name + "." + symbol->hash);
				symbol->value = vi;

				ai++;
				vi++;
			}

			/* Now import any upvalues. */

			typename FunctionSymbol::LocalsMap::const_iterator li = function->locals.begin();

			while (li != function->locals.end())
			{
				if (li->first != li->second)
				{
					assert(vi != f->arg_end());
					VariableSymbol* symbol = li->second;
					vi->setName(symbol->name + "." + symbol->hash);
					symbol->value = vi;

					vi++;
				}

				li++;
			}

			assert(vi == f->arg_end());
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
	using ASTNode::getFunction;
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
		function->parent = getFunction();

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
	CallableSymbol* function;

	using ASTNode::position;
	using ASTNode::getFrame;
	using ASTNode::getFunction;

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
		function = symbol->isCallable();
		if (!function)
		{
			std::stringstream s;
			s << "attempt to call '" << id << "', which is not a function";
			throw CompilationException(position.formatError(s.str()));
		}

		for (typename vector<ASTNode*>::const_iterator i = arguments.begin(),
				e = arguments.end(); i != e; i++)
		{
			(*i)->resolveVariables(compiler);
		}

		/* Ensure that any upvalues that the function wants exist in this
		 * function.
		 */

		FunctionSymbol* callee = function->isFunction();
		if (callee)
		{
			FunctionSymbol* caller = getFunction();
			for (typename FunctionSymbol::LocalsMap::const_iterator i = callee->locals.begin(),
					e = callee->locals.end(); i != e; i++)
			{
				if (i->first != i->second)
					caller->importUpvalue(compiler, i->first);
			}
		}
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		function->checkParameterCount(compiler, arguments.size());

		/* Formal parameters. */

		vector<llvm::Value*> parameters;
		for (typename vector<ASTNode*>::const_iterator i = arguments.begin(),
				e = arguments.end(); i != e; i++)
		{
			llvm::Value* v = (*i)->codegen(compiler);
			parameters.push_back(v);
		}

		/* ...followed by imported upvalues. */

		FunctionSymbol* callee = function->isFunction();
		if (callee)
		{
			FunctionSymbol* caller = getFunction();
			for (typename FunctionSymbol::LocalsMap::const_iterator i = callee->locals.begin(),
					e = callee->locals.end(); i != e; i++)
			{
				if (i->first != i->second)
				{
					VariableSymbol* s = caller->locals[i->first];
					assert(s);
					parameters.push_back(s->value);
				}
			}
		}

		compiler.position = position;
		return function->emitCall(compiler, parameters);
	}
};

struct ASTCondition : public ASTNode
{
	ASTNode* condition;
	ASTNode* trueval;
	ASTNode* falseval;

	using ASTNode::position;

	ASTCondition(const Position& position, ASTNode* condition,
			ASTNode* trueval, ASTNode* falseval):
		ASTNode(position),
		condition(condition), trueval(trueval), falseval(falseval)
	{
		condition->parent = trueval->parent = falseval->parent = this;
	}

	void resolveVariables(Compiler& compiler)
	{
		condition->resolveVariables(compiler);
		trueval->resolveVariables(compiler);
		falseval->resolveVariables(compiler);
	}

	llvm::Value* codegen(Compiler& compiler)
	{
		llvm::Value* cv = condition->codegen_to_boolean(compiler);

		llvm::BasicBlock* bb = compiler.builder.GetInsertBlock();

		llvm::BasicBlock* trueblock = llvm::BasicBlock::Create(
				compiler.context, "", bb->getParent());
		llvm::BasicBlock* falseblock = llvm::BasicBlock::Create(
				compiler.context, "", bb->getParent());
		llvm::BasicBlock* mergeblock = llvm::BasicBlock::Create(
				compiler.context, "", bb->getParent());

		compiler.builder.CreateCondBr(cv, trueblock, falseblock);

		compiler.builder.SetInsertPoint(trueblock);
		llvm::Value* trueresult = trueval->codegen(compiler);
		trueblock = compiler.builder.GetInsertBlock();
		compiler.builder.CreateBr(mergeblock);

		compiler.builder.SetInsertPoint(falseblock);
		llvm::Value* falseresult = falseval->codegen(compiler);
		falseblock = compiler.builder.GetInsertBlock();
		compiler.builder.CreateBr(mergeblock);

		if (trueresult->getType() != falseresult->getType())
		{
			std::stringstream s;
			s << "the true and false value of a conditional must be the same type";
			throw CompilationException(position.formatError(s.str()));
		}

		compiler.builder.SetInsertPoint(mergeblock);
		llvm::PHINode* phi = compiler.builder.CreatePHI(trueresult->getType(), 2);
		phi->addIncoming(trueresult, trueblock);
		phi->addIncoming(falseresult, falseblock);
		return phi;
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

#endif
