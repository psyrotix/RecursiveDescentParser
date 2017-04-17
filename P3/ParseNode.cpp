/*
 * ParseNode.cpp
 *
 *  Created on: Apr 2, 2017
 *      Author: gerardryan
 */
#include <vector>
#include <regex>
#include "ParseNode.h"
#include "polylex.h"

using namespace std;
// We want to use our getToken routine unchanged... BUT we want to have the ability
// to push back a token if we read one too many; this is our "lookahead"
// so we implement a "wrapper" around getToken

static bool pushedBack = false;
static Token	pushedToken;

Token GetToken(istream& in) {
    
	return getToken(in);
}
Token getToken(istream& in){
    enum State { START, INID, INSTRING, INICONST, INFCONST, INCOMMENT};
    
    State lexstate = START;
    std::string lexeme = "";
    
    for(;;) {
        char ch = in.get();
        
        if( in.eof() || in.bad() )
            break;
        
        if( ch == '\n' && lexstate == START ) currentLine++;
        
        switch( lexstate ) {
            case START:
                if( isspace(ch) )
                    break;
                if( isalpha(ch) ) {
                    lexstate = INID;
                    lexeme += ch;
                }
                else if( isdigit(ch) ) {
                    lexstate = INICONST;
                    lexeme += ch;
                }
                else if(ch == ';'){
                    return Token(SC, ";");
                }
                else if(ch == '+'){
                    return Token(PLUS, "+");
                }
                else if(ch == '-'){
                    return Token(MINUS, "-");
                }
                else if(ch == '*'){
                    return Token(STAR, "*");
                }
                else if(ch == '['){
                    return Token(LSQ, "[");
                }
                else if(ch == ']'){
                    return Token(RSQ, "]");
                }
                else if(ch == '('){
                    return Token(LPAREN, "(");
                }
                else if(ch == ')'){
                    return Token(RPAREN, ")");
                }
                else if( ch == '#' ) {
                    lexstate = INCOMMENT;
                    lexeme += ch;
                }
                else if( ch == '{' ) {
                    return Token(LBR);
                } else if( ch == '}' ) {
                    return Token(RBR);
                }
                else if( ch == '"' ) {
                    lexstate = INSTRING;
                }
                else if( ch == '-' ) {
                    // maybe minus? maybe leading sign on a number?
                    if( isdigit(in.peek()) ) {
                        lexstate = INICONST;
                    }
                    else return Token(MINUS);
                } 
                else {
                    return Token(ERR,lexeme);
                }
                
                break;
                
            case INID:
                if( !isalnum(ch) ) {
                    in.putback(ch);
                    if(lexeme == "set"){
                        return Token(SET, lexeme);
                    } else if (lexeme == "print"){
                        return Token(PRINT, lexeme);
                    } else
                    return Token(ID, lexeme);
                }
                lexeme += ch;
                break;
                
            case INSTRING:
                if( ch == '"' ) {
                    lexeme += ch;
                    return Token(STRING, lexeme);
                }
                else if( ch == '\n' ) {
                    return Token(ERR, lexeme);
                }
                lexeme += ch;
                break;
                
            case INICONST:
                if( isdigit(ch) ) {
                    lexeme += ch;
                    
                }
                else if( ch == '.' ) {
                    lexeme += ch;
                    if( isdigit(in.peek()) )
                        lexstate = INFCONST;
                    else
                        return Token(ERR, lexeme);
                } else if(ch == ','){
                    return Token(ICONST, lexeme);
                }
                else {
                    in.putback(ch);
                    return Token(ICONST, lexeme);
                }
                
                break;
                
            case INFCONST:
                if( isdigit(ch) ) {
                    lexeme += ch;
                }
                else {
                    in.putback(ch);
                    return Token(FCONST, lexeme);
                }
                
                break;
                
            case INCOMMENT:
                if( ch == '\n' ) {
                    currentLine++;
                    lexstate = START;
                    return Token(NEWLINE);
                }
                break;

            }
        }
        // handle getting DONE or ERR when not in start state
        if(in.eof()){
            if( lexstate == START ) return Token(DONE);
            if( lexstate == INSTRING) return Token(DONE);
            if( lexstate == INCOMMENT) return Token(DONE);
        }
        
        return Token(ERR, lexeme);
}
static ParseNode* GetOneCoeff(istream& in) {
    Token t = GetToken(in);
    if( t == ICONST ) {
        return new Iconst(std::stoi(t.getLexeme()));
    }
    else if( t == FCONST ) {
        return new Fconst(std::stof(t.getLexeme()));
    }
    else
        return 0;
}

void PutBackToken(Token& t) {
	if( pushedBack ) {
		cout << "You can only push back one token!" << endl;
		exit(0);
	}

	pushedBack = true;
	pushedToken = t;
}

// handy function to print out errors
void error(string s) {
	cout << "PARSE ERROR: " << currentLine << " " << s << endl;
	++globalErrorCount;
}

// Prog := Stmt | Stmt Prog
ParseNode *Prog(istream& in) {
	ParseNode *stmt = Stmt(in);

	if( stmt != 0 )
		return new StatementList(stmt, Prog(in));
	return 0;
}

// Stmt := Set ID Expr SC | PRINT Expr SC
ParseNode *Stmt(istream& in) {
	Token cmd = GetToken(in);

	if( cmd == SET ) {
		Token idTok = GetToken(in);
		if( idTok != ID ) {
			error("Identifier required after set");
			return 0;
		}
		ParseNode *exp = Expr(in);
		if( exp == 0 ) {
			error("expression required after id in set");
			return 0;
		}
		if( GetToken(in) != SC ) {
			error("semicolon required");
			return 0;
		}

		return new SetStatement(idTok.getLexeme(), exp);
	}
	else if( cmd == PRINT ) {
		ParseNode *exp = Expr(in);
		if( exp == 0 ) {
			error("expression required after id in print");
			return 0;
		}
		if( GetToken(in) != SC ) {
			error("semicolon required");
			return 0;
		}

		return new PrintStatement(exp);
	}
	else
		PutBackToken(cmd);
	return 0;
}

ParseNode *Expr(istream& in) {
	ParseNode *t1 = Term(in);
	if( t1 == 0 )
	return 0;
	for(;;) {
	Token op = GetToken(in);
	if( op != PLUS && op != MINUS ) {
	PutBackToken(op);
	return t1;
	}
	ParseNode *t2 = Expr(in);
	if( t2 == 0 ) {
		error("expression required after + or - operator");
		return 0;
	}
	// combine t1 and t2 together
	if( op == PLUS )
		t1 = new PlusOp(t1,t2);
	else
		t1 = new MinusOp(t1,t2);
	}
	// should never get here...
	return 0;
}

ParseNode *Term(istream& in) {
	return 0;
}

// Primary :=  ICONST | FCONST | STRING | ( Expr ) | Poly
ParseNode *Primary(istream& in) {
	// check tokens... or call Poly
	return 0;
}

// Poly := LCURLY Coeffs RCURLY { EvalAt } | ID { EvalAt }
ParseNode *Poly(istream& in) {
	// note EvalAt is optional
	return 0;
}

// notice we don't need a separate rule for ICONST | FCONST
// this rule checks for a list of length at least one
ParseNode *Coeffs(istream& in) {
    std::vector<ParseNode *> coeffs;
    ParseNode *p = GetOneCoeff(in);
    if( p == 0 )
        return 0;
    coeffs.push_back(p);
    while( true ) {
        Token t = GetToken(in);
        if( t != COMMA ) {
            PutBackToken(t);
            break;
        }
        p = GetOneCoeff(in);
        if( p == 0 ) {
            error("Missing coefficient after comma");
            return 0;
        }
        coeffs.push_back(p);
    }
    return new Coefficients(coeffs); // Coefficients class must take vector
}

ParseNode *EvalAt(istream& in) {
	return 0;
}