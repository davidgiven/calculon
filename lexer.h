#ifndef CALCULON_LEXER_H
#define CALCULON_LEXER_H

#ifndef CALCULON_H
#error Don't include this, include calculon.h instead.
#endif

template <typename Real>
class Lexer
{
public:
	enum Token
	{
		INVALID,
		EOF,
		IDENTIFIER,
		OPERATOR,
		NUMBER,
		EQUALS,
		LET,
		AND,
		OR,
		NOT,
		VECTOR,
		REAL
	};

	class LexerException : public CompilationException
	{
	public:
		LexerException(const string& what, Lexer& lexer):
			CompilationException(lexer.formatError(what))
		{
		}
	};

private:
	std::istream& _data;
	Token _token;
	string _idValue;
	Real _realValue;
	int _line;
	int _column;

public:
	Lexer(std::istream& data):
		_data(data),
		_token(INVALID),
		_line(1),
		_column(1)
	{
		/* Prime the lexer with the first token. */
		next();
	}

	Token token() const
	{
		assert(_token != INVALID);
		return _token;
	}

	string id() const
	{
		assert((_token == IDENTIFIER) || (_token == OPERATOR));
		return _idValue;
	}

	Real real() const
	{
		assert(_token == NUMBER);
		return _realValue;
	}

	Token next()
	{
		int c;
		for (;;)
		{
			c = _data.peek();
			if (!std::isspace(c))
				break;
			consume();
		}
		if (_data.eof())
		{
			_token = EOF;
			return EOF;
		}

		if (std::isdigit(c))
			readNumber();
		else if (isid(c))
			readId();
		else if (std::ispunct(c))
			readOperator();
		else
		{
			std::stringstream s;
			s << "Unknown character '" << (char)c << "'";
			error(s.str());
		}

		return _token;
	}

	void position(int& line, int& column)
	{
		line = _line;
		column = _column;
	}

	string formatError(const string& what)
	{
		std::stringstream s;
		s << what
			<< " at " << _line << ":" << _column;
		return s.str();
	}

	void error(const string& s)
	{
		throw LexerException(s, *this);
	}

private:
	bool isid(int c)
	{
		return std::isalpha(c) || (c == '_');
	}

	bool iscontinuedid(int c)
	{
		return isid(c) || std::isdigit(c);
	}

	int consume()
	{
		int c;
		do
			c = _data.get();
		while (c == '\r');

		if (c == '\n')
		{
			_line++;
			_column = 1;
		}
		else
			_column++;

		return c;
	}

	void readNumber()
	{
		std::ios::streampos pos = _data.tellg();

		_data >> _realValue;
		if (!_data)
			error("invalid number syntax");

		_column += _data.tellg() - pos;
		_token = NUMBER;
	}

	void readId()
	{
		std::stringstream s;

		do
		{
			int c = consume();
			s << (char) c;
		}
		while (iscontinuedid(_data.peek()));

		_idValue = s.str();

		if (_idValue == "let")
			_token = LET;
		else if (_idValue == "and")
			_token = AND;
		else if (_idValue == "or")
			_token = OR;
		else if (_idValue == "not")
			_token = NOT;
		else if (_idValue == "vector")
			_token = VECTOR;
		else if (_idValue == "real")
			_token = REAL;
		_token = IDENTIFIER;
	}

	void readOperator()
	{
		int c = consume();
		_idValue = (char) c;

		int p = _data.peek();
		switch (c)
		{
			case '.':
				if (std::isdigit(p))
				{
					/* Oops, this is really a number */
					_data.putback('.');
					readNumber();
					return;
				}
				break;

			case '=':
			case '<':
			case '>':
				if (p == '=')
				{
					consume();
					_idValue += '=';
				}
				break;
		}

		_token = OPERATOR;
	}
};


#endif
