#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

char *p, *lp, // current position in source code
     *data;   // data/bss pointer

int *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    ival,     // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug;    // print executed instructions

// tokens and classes (operators last and in precedence order)
/*
Id = identifier user-defined identifier
//运算符
Assign  =
Cond    ?
Lor     ||
Lan     &&
Or      |
Xor     ^
And     &
Eq      ==
Ne      !=
Lt      <
Gt      >
Le      <=
Ge      >=
Shl     <<
Shr     >>
Add     +
Sub     -
Mul     *
Div     /
Mod     %
Inc     ++
Dec     --
Brak    [
*/
/*
id[Class] =
Num: constant value
Fun: function
Sys: system call
Glo: global variable
Loc: local variable
*/
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// opcodes
enum { LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

// types
enum { CHAR, INT, PTR };

// identifier offsets (since we can't create an ident struct)
// Structs are not implemented, so the space pointed by [id] is split into Idsz sized chunks to simulate structs
// When id points to the beginning of the block, id[0] == id[Tk], accessing the data of the Tk member (usually a pointer)
// Name points to the Name of this identifier
// Type is the data type (such as the return value type), such as CHAR, INT, INT+PTR
// Class is a type, such as Num (constant value), Fun (function), Sys (system call), Glo global variable, Loc local variable
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz };

//lexical analysis
void next()
{
  char *pp;
  // Use a loop to ignore whitespace characters, but characters which can not be recognized by the lexer are all considered as whitespace characters, such as '@', '$'
  while (tk = *p) {
    ++p;
    if (tk == '\n') {
      if (src) {
        printf("%d: %.*s", line, p - lp, lp);
        lp = p;
        while (le < e) {
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                           "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                           "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT,"[*++le * 5]);
          if (*le <= ADJ) printf(" %d\n", *++le); else printf("\n"); 
        }
      }
      ++line;
    }
    else if (tk == '#') {//# would be a single-line comment symbol, handle #include, etc.
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
      pp = p - 1; 
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        tk = tk * 147 + *p++;
      tk = (tk << 6) + (p - pp); 
      id = sym;
      while (id[Tk]) {
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) {
          //find the same name, then tk = id[Tk] (look at id as a structure, access its Tk members, see above for explanation)
          tk = id[Tk];
          return;
        }
        id = id + Idsz;//Continue to loop through the identifier table
      }
      id[Name] = (int)pp;
      id[Hash] = tk;//hash value
      tk = id[Tk] = Id; //token's type is identifier
      return;
    }
    else if (tk >= '0' && tk <= '9') {// The first digit is a number, which is considered to be a numerical value
      if (ival = tk - '0') { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; }//If the first digit is not 0, it is considered to be a decimal number
      else if (*p == 'x' || *p == 'X') {//The first bit is 0 and starts with x, which is considered to be a hexadecimal number
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; }//considered octal
      tk = Num;//token is a numeric type, return
      return;
    }
    else if (tk == '/') {
      if (*p == '/') {//Start with two '/', single-line comment
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else {
        tk = Div; //division
        return;
      }
    }
    else if (tk == '\'' || tk == '"') {//Because it starts with quotation marks, it is considered to be a character (string)
      pp = data;
      while (*p != 0 && *p != tk) {//until a matching quote is found
        if ((ival = *p++) == '\\') {
          if ((ival = *p++) == 'n') ival = '\n';// the '\n' is considered to be '\n' while other '\' would be ignored directly
        }
        if (tk == '"') *data++ = ival;//If it starts with double quotes, it is considered to be a string, and the characters are copied to data
      }
      ++p;
      if (tk == '"') ival = (int)pp; else tk = Num;//Double quotes mean that ival points to the beginning of the string in data, and single quotes mean that it is a number
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; }
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else tk = Sub; return; }
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }
    else if (tk == '&') { if (*p == '&') { ++p; tk = Lan; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Brak; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}
//expression analysis
//lev represents an operator, because each operator token is arranged in order of priority, so a larger lev indicates a higher priority

void expr(int lev)
{
  int t, *d;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *++e = IMM; *++e = ival; next(); ty = INT; } //Take immediate value directly as expression value
  else if (tk == '"') { //string
    *++e = IMM; *++e = ival; next();
    while (tk == '"') next();
    data = (char *)((int)data + sizeof(int) & -sizeof(int));//byte aligned to int
    ty = PTR;
  }
  else if (tk == Sizeof) {
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof\n", line); exit(-1); }
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
    while (tk == Mul) { next(); ty = ty + PTR; }//Multi-level pointer, add PTR for each extra level
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof\n", line); exit(-1); }
    *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);//Except Char is one byte, int and multilevel pointers are int size
    ty = INT;
  }
  else if (tk == Id) {//identifier
    d = id; next();
    if (tk == '(') {//function
      next();
      t = 0;//number of formal parameters
      while (tk != ')') { expr(Assign); *++e = PSH; ++t; if (tk == ',') next(); }//Calculate the value of the actual parameter, push the stack (pass the parameter)
      next();
      if (d[Class] == Sys) *++e = d[Val]; //System calls, such as malloc, memset, d[val] are opcode
      else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; } //User-defined function, d[Val] is the function entry address
      else { printf("%d: bad function call\n", line); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; }//Because the stack is used to pass parameters, adjust the stack
      ty = d[Type];//function return type
    }
    else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; }/d[Class] == Num, to handle enumeration (only enumeration is Class==Num)
    else {//variable
      //The variable takes the address first and then LC/LI
      if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; }//Take the address, d[Val] is the local variable offset
      else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }//Take the address, d[Val] is the global variable pointer
      else { printf("%d: undefined variable\n", line); exit(-1); }
      *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
    }
  }
  else if (tk == '(') {
    next();
    if (tk == Int || tk == Char) {//Force type conversion
      t = (tk == Int) ? INT : CHAR; next();
      while (tk == Mul) { next(); t = t + PTR; }//pointer
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }
      expr(Inc); //high priority
      ty = t;
    }
    else { //General syntax parentheses
      expr(Assign);
      if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    }
  }
  else if (tk == Mul) {//take pointer to value
    next(); expr(Inc);//high priority
    if (ty > INT) ty = ty - PTR; else { printf("%d: bad dereference\n", line); exit(-1); }
    *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) {//&,address fetch operation
    next(); expr(Inc);
    if (*e == LC || *e == LI) --e;//When token is a variable, the address is taken first and then LI/LC, so --e becomes the address to a
    else { printf("%d: bad address-of\n", line); exit(-1); }
    ty = ty + PTR;
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; }//!x is equivalent to x==0
  else if (tk == '~') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; }//~x is equivalent to x^-1
  else if (tk == Add) { next(); expr(Inc); ty = INT; }
  else if (tk == Sub) {
    next(); *++e = IMM;
    if (tk == Num) { *++e = -ival; next(); } //value, negative
    else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; }//multiply by -1
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) {//process ++x,--x
    t = tk; next(); expr(Inc);
    if (*e == LC) { *e = PSH; *++e = LC; }//The address is pushed to the stack (used by SC/SI below), and then the number is fetched
    else if (*e == LI) { *e = PSH; *++e = LI; }
    else { printf("%d: bad lvalue in pre-increment\n", line); exit(-1); }
    *++e = PSH;//push value onto stack
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);//The pointer is added or subtracted by one word length, otherwise it is added or subtracted by 1
    *++e = (t == Inc) ? ADD : SUB;//operation
    *++e = (ty == CHAR) ? SC : SI;//store back variable
  }
  else { printf("%d: bad expression\n", line); exit(-1); }

  //tk is ASCII code will not exceed Num=128
  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
    t = ty;//ty may change during recursion, so back up the currently processed expression type
    if (tk == Assign) { //assign
      next();
      if (*e == LC || *e == LI) *e = PSH; //The left side is processed by the variable part in tk=Id, and the address is pushed onto the stack
      else { printf("%d: bad lvalue in assignment\n", line); exit(-1); }
      expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;//Get the value of the expr, as the result of a=expr
    }
    else if (tk == Cond) {
      next();
      *++e = BZ; d = ++e;
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon\n", line); exit(-1); }
      *d = (int)(e + 3); *++e = JMP; d = ++e;
      expr(Cond);
      *d = (int)(e + 1);
    }
    else if (tk == Lor) { next(); *++e = BNZ; d = ++e; expr(Lan); *d = (int)(e + 1); ty = INT; }//If the left side of the logical Or operator is true, the expression is true, and the value on the right side of the operator is not calculated.
    else if (tk == Lan) { next(); *++e = BZ;  d = ++e; expr(Or);  *d = (int)(e + 1); ty = INT; }//Logic And
    else if (tk == Or)  { next(); *++e = PSH; expr(Xor); *++e = OR;  ty = INT; }//Push the current value, calculate the right value of the operator, and then operate with the current value who is in the stack;
    else if (tk == Xor) { next(); *++e = PSH; expr(And); *++e = XOR; ty = INT; }//The lev in expr indicates which operator in the recursive function must be the most associative
    else if (tk == And) { next(); *++e = PSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)  { next(); *++e = PSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)  { next(); *++e = PSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)  { next(); *++e = PSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)  { next(); *++e = PSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)  { next(); *++e = PSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)  { next(); *++e = PSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl) { next(); *++e = PSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr) { next(); *++e = PSH; expr(Add); *++e = SHR; ty = INT; }
    else if (tk == Add) {
      next(); *++e = PSH; expr(Mul);
      if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }//handle pointers
      *++e = ADD;
    }
    else if (tk == Sub) {
      next(); *++e = PSH; expr(Mul);
      if (t > PTR && t == ty) { *++e = SUB; *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; }//pointer subtraction
      else if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; }//pointer decrement value
      else *++e = SUB;
    }
    else if (tk == Mul) { next(); *++e = PSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PSH; expr(Inc); *++e = MOD; ty = INT; }
    else if (tk == Inc || tk == Dec) {//process x++,x--
      if (*e == LC) { *e = PSH; *++e = LC; }
      else if (*e == LI) { *e = PSH; *++e = LI; }
      else { printf("%d: bad lvalue in post-increment\n", line); exit(-1); }
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? ADD : SUB;//First increment/decrement
      *++e = (ty == CHAR) ? SC : SI;//save in memory
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? SUB : ADD;//Operate in the opposite way to ensure that the subsequent increment/decrement does not affect the evaluation of this expression
      next();
    }
    else if (tk == Brak) {//array subscript
      next(); *++e = PSH; expr(Assign);//save array pointer, calculate subscript
      if (tk == ']') next(); else { printf("%d: close bracket expected\n", line); exit(-1); }
      if (t > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  } //when t==PTR, Char = 0
      else if (t < PTR) { printf("%d: pointer type expected\n", line); exit(-1); }
      *++e = ADD;
      *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
    }
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

//The part of the analysis function other than the declaration, that is, the parsing
void stmt()
{
  int *a, *b;

  if (tk == If) {
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; //branch if zero
    b = ++e;//branch address pointer
    stmt(); // continue analysis
    if (tk == Else) {
      *b = (int)(e + 3); // The e + 3 position is the else start position
      *++e = JMP; // Insert JMP before the if statement else to skip the Else part
      b = ++e; // JMP aim
      next();
      stmt();//Analyze the else part
    }
    *b = (int)(e + 1);//The if statement ends, whether it is the jump target of if BZ or the jump target of JMP before else
  }
  else if (tk == While) {//loop
    next();
    a = e + 1; // While loop body start address
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; b = ++e;//b = While, address after statement ends
    stmt();//Handling While Statement Body
    *++e = JMP; *++e = (int)a;//Unconditionally jump to the start of the While statement (including the code of the loop condition) to realize the loop
    *b = (int)(e + 1);//BZ jump target (end of loop)
  }
  else if (tk == Return) {
    next();
    if (tk != ';') expr(Assign);//Calculate return value
    *++e = LEV;
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
  else if (tk == '{') {//compound statement
    next();
    while (tk != '}') stmt();
    next();
  }
  else if (tk == ';') {
    next();
  }
  else {
    expr(Assign);//The general statement is considered to be an assignment statement/expression
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
}

int main(int argc, char **argv)
{
  //fd file descriptor: File Description
  //bt basetype
  //poolsz: a range of pool sizes
  int fd, bt, ty, poolsz, *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers: virtualizer registers
  int i, *t; // temps

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }//open a file

  poolsz = 256*1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }//Symbol table
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }//// current position in emitted code
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }//data segment
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }//stack

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);

  // Use the lexical analyzer to put these keywords into the symbol table first
  p = "char else enum if int return sizeof while "
      "open read close printf malloc memset memcmp exit void main";
  // Add the keyword, id[Tk] is modified to be the same as Enum
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  //Add the symbols (system functions, etc.) defined in the [library] into the Class and assign the value to Sys
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  // void is considered to be char
  next(); id[Tk] = Char; // handle void type
  // Record the symbolic id of the main function
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0;//Set the end of the string to 0
  close(fd);

  // parse declarations
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next(); //already has bt == INT
    else if (tk == Char) { next(); bt = CHAR; }//char variable
    else if (tk == Enum) {
      next();
      if (tk != '{') next();
      if (tk == '{') {
        next();
        i = 0; //Enum starts from 0 by default
        while (tk != '}') {
          if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; } //Error if not Identifier
          next();
          if (tk == Assign) { // Find assignment statements like enum { Num = 128 }
            next();
            if (tk != Num) { printf("%d: bad enum initializer\n", line); return -1; }
            i = ival;
            next();
          }
          //id has already been processed by the next function
          id[Class] = Num; id[Type] = INT; id[Val] = i++;
          if (tk == ',') next();
        }
        next();
      }
    }
    //Enum finishes processing tk == ';', skip the following
    while (tk != ';' && tk != '}') {
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; } // tk == Mul Indicates that it starts with *, it is a pointer type, and the type plus PTR indicates what type of pointer
      if (tk != Id) { printf("%d: bad global declaration\n", line); return -1; }
      if (id[Class]) { printf("%d: duplicate global definition\n", line); return -1; } //Duplicate global variable definitions
      next();
      id[Type] = ty; //assignment type
      if (tk == '(') { // function
        id[Class] = Fun;//type is function
        id[Val] = (int)(e + 1); //function pointer's offset/address in bytecode
        next(); i = 0;
        while (tk != ')') {//parameter list
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration\n", line); return -1; }
          if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; } //function parameters are local variables
          //Backup symbol information, before entering the function context
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          id[HVal]   = id[Val];   id[Val] = i++;//local variable number
          next();
          if (tk == ',') next();
        }
        next();
        if (tk != '{') { printf("%d: bad function definition\n", line); return -1; }
        loc = ++i; //local variable offset
        next();
        while (tk == Int || tk == Char) { //Variable declaration inside a function
          bt = (tk == Int) ? INT : CHAR;
          next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }//Handling pointers
            if (tk != Id) { printf("%d: bad local declaration\n", line); return -1; }
            if (id[Class] == Loc) { printf("%d: duplicate local definition\n", line); return -1; }
            //Backup symbol information
            id[HClass] = id[Class]; id[Class] = Loc;
            id[HType]  = id[Type];  id[Type] = ty;
            id[HVal]   = id[Val];   id[Val] = ++i; //store variable offset
            next();
            if (tk == ',') next();
          }
          next();
        }
        *++e = ENT; *++e = i - loc;//Number of function local variables
        while (tk != '}') stmt();//
        *++e = LEV;//function return
        id = sym; // unwind symbol table locals
        while (id[Tk]) {
          //restore symbol information
          if (id[Class] == Loc) {
            id[Class] = id[HClass];
            id[Type] = id[HType];
            id[Val] = id[HVal];
          }
          id = id + Idsz;
        }
      }
      else {
        id[Class] = Glo;//global variable
        id[Val] = (int)data;//Allocate memory in the data segment for global variables
        data = data + sizeof(int);
      }
      if (tk == ',') next();
    }
    next();
  }

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // setup stack
  sp = (int *)((int)sp + poolsz);
  *--sp = EXIT; // call exit if main returns
  *--sp = PSH; t = sp;
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)t;

  // run...
  cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) {
      printf("%d> %.4s", cycle,
        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
         "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT,"[i * 5]);
      if (i <= ADJ) printf(" %d\n", *pc); else printf("\n");
    }
    if      (i == LEA) a = (int)(bp + *pc++);                             // load local address
    else if (i == IMM) a = *pc++;                                         // load global address or immediate
    else if (i == JMP) pc = (int *)*pc;                                   // jump
    else if (i == JSR) { *--sp = (int)(pc + 1); pc = (int *)*pc; }        // jump to subroutine
    else if (i == BZ)  pc = a ? pc + 1 : (int *)*pc;                      // branch if zero
    else if (i == BNZ) pc = a ? (int *)*pc : pc + 1;                      // branch if not zero
    else if (i == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }     // enter subroutine
    else if (i == ADJ) sp = sp + *pc++;                                   // stack adjust
    else if (i == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // leave subroutine
    else if (i == LI)  a = *(int *)a;                                     // load int
    else if (i == LC)  a = *(char *)a;                                    // load char
    else if (i == SI)  *(int *)*sp++ = a;                                 // store int
    else if (i == SC)  a = *(char *)*sp++ = a;                            // store char
    else if (i == PSH) *--sp = a;                                         // push

    else if (i == OR)  a = *sp++ |  a;
    else if (i == XOR) a = *sp++ ^  a;
    else if (i == AND) a = *sp++ &  a;
    else if (i == EQ)  a = *sp++ == a;
    else if (i == NE)  a = *sp++ != a;
    else if (i == LT)  a = *sp++ <  a;
    else if (i == GT)  a = *sp++ >  a;
    else if (i == LE)  a = *sp++ <= a;
    else if (i == GE)  a = *sp++ >= a;
    else if (i == SHL) a = *sp++ << a;
    else if (i == SHR) a = *sp++ >> a;
    else if (i == ADD) a = *sp++ +  a;
    else if (i == SUB) a = *sp++ -  a;
    else if (i == MUL) a = *sp++ *  a;
    else if (i == DIV) a = *sp++ /  a;
    else if (i == MOD) a = *sp++ %  a;

    else if (i == OPEN) a = open((char *)sp[1], *sp);
    else if (i == READ) a = read(sp[2], (char *)sp[1], *sp);
    else if (i == CLOS) a = close(*sp);
    else if (i == PRTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); }
    else if (i == MALC) a = (int)malloc(*sp);
    else if (i == MSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    else if (i == MCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
  }
}
