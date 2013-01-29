/* Calculon © 2013 David Given
 * This code is made available under the terms of the Simplified BSD License.
 * Please see the COPYING file for the full license text.
 */

#ifndef CALCULON_LEXER_H
#define CALCULON_LEXER_H

#ifndef CALCULON_H
#error "Don't include this, include calculon.h instead."
#endif

template <typename Real>
class Lexer
{
public:
	enum
	{
		INVALID,
		ENDOFFILE,
		NUMBER,
		IDENTIFIER,
		OPERATOR,
		EQUALS,
		OPENPAREN,
		CLOSEPAREN,
		OPENBLOCK,
		CLOSEBLOCK,
		COMMA,
		COLON,
		SEMICOLON,
		DOT
	};

	class LexerException : public CompilationException
	{
	public:
		LexerException(const string& what, Lexer& lexer):
			CompilationException(lexer.position().formatError(what))
		{
		}
	};

private:
	std::istream& _data;
	int _token;
	string _idValue;
	Real _realValue;
	Position _tokenPos;
	Position _pos;

public:
	Lexer(std::istream& data):
		_data(data),
		_token(INVALID)
	{
		_pos.line = 1;
		_pos.column = 1;

		/* Prime the lexer with the first token. */
		next();
	}

	int token() const
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

	int next()
	{
		do
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
				_token = ENDOFFILE;
				return ENDOFFILE;
			}

			_tokenPos = _pos;

			if (std::isdigit(c))
				read_number();
			else if (isid(c))
				read_id();
			else if (std::ispunct(c))
				read_operator();
			else
			{
				std::stringstream s;
				s << "Unknown character '" << (char)c << "'";
				error(s.str());
			}
		}
		while (_token == INVALID);

		return _token;
	}

	Position position() const
	{
		return _tokenPos;
	}

	void error(const string& s)
	{
		throw LexerException(s, *this);
	}

	const char* tokenname(int token)
	{
		switch (token)
		{
			case ENDOFFILE:  return "EOF";
			case NUMBER:     return "number";
			case IDENTIFIER: return "identifier";
			case OPERATOR:   return "operator";
			case EQUALS:     return "'='";
			case OPENPAREN:  return "'('";
			case CLOSEPAREN: return "')'";
			case OPENBLOCK:  return "'['";
			case CLOSEBLOCK: return "']'";
			case COMMA:      return "','";
			case COLON:      return "':'";
			case SEMICOLON:  return "';'";
			case DOT:        return "'.'";

			default:
				assert(false);
		}
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
			_pos.line++;
			_pos.column = 1;
		}
		else
			_pos.column++;

		return c;
	}

	void read_number()
	{
		std::ios::streampos pos = _data.tellg();

		_data >> _realValue;
		if (!_data)
			error("invalid number syntax");

		_pos.column += _data.tellg() - pos;
		_token = NUMBER;
	}

	void read_id()
	{
		std::stringstream s;

		do
		{
			int c = consume();
			s << (char) c;
		}
		while (iscontinuedid(_data.peek()));

		_idValue = s.str();

		if (_idValue == "and")
			_token = OPERATOR;
		else if (_idValue == "or")
			_token = OPERATOR;
		else if (_idValue == "not")
			_token = OPERATOR;
		else
			_token = IDENTIFIER;
	}

	void read_multiline_comment()
	{
		for (;;)
		{
			if (_data.eof())
				error("unexpected end of file in multiline comment");

			int c = consume();
			if ((c == '*') && (_data.peek() == '/'))
			{
				consume();
				break;
			}
		}
		_token = INVALID;
	}

	void read_singleline_comment()
	{
		int line = _pos.line;
		do
		{
			consume();
		}
		while (!_data.eof() && (line == _pos.line));
		_token = INVALID;
	}

	void read_operator()
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
					read_number();
					return;
				}
				break;

			case '/':
				if (p == '*')
				{
					read_multiline_comment();
					return;
				}
				else if (p == '/')
				{
					read_singleline_comment();
					return;
				}
				break;

			case '=':
			case '<':
			case '>':
			case '!':
				if (p == '=')
				{
					consume();
					_idValue += '=';
				}
				break;
		}

		if (_idValue == "(")
			_token = OPENPAREN;
		else if (_idValue == ")")
			_token = CLOSEPAREN;
		else if (_idValue == "[")
			_token = OPENBLOCK;
		else if (_idValue == "]")
			_token = CLOSEBLOCK;
		else if (_idValue == ":")
			_token = COLON;
		else if (_idValue == ",")
			_token = COMMA;
		else if (_idValue == ".")
			_token = DOT;
		else if (_idValue == ";")
			_token = SEMICOLON;
		else
			_token = OPERATOR;
	}
};


#endif
