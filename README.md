# PL/0 Language Compiler
# Inroduction

I developed a compiler of four parts, including lexical analysis, syntax analysis, virtual machine, and symbol table, which could translate PL/0 language (a subset of C Language) into executable 8086 Assembly language code

(1) Developed lexical analyzer to **preprocess source strings to reduce the complexity of the parser** and **convert string to the internal representation structure**.

(2) Used the **Recursive-Descent** method and **Operator-Precedence** method to deal with variable, function, statement, and expressions; and then **generate a syntax tree** from the token stream obtained by lexical analysis.

(3) The analytic functions in the virtual machine could **convert the syntax tree to the object code**.
# Virtual Machine Analysis
Lexical analysis and syntax analysis are simpler, so here I mainly explain the virtual machine.

``int *pc, *sp, *bp, a, cycle; /* define the register */ ``

* pc is the program counter/instruction pointer

* sp is the stack register, pointing to the top of the stack. Note that the stack grows from high ground to low address

* bp is the base register

* a is the accumulator

* cycle is the execution instruction count

## Instruction Set

* LEA removes the local variable address, takes [PC+1] as address, takes bp as the base address, and loads the address into the accumulator a

* IMM[PC+1] is loaded into accumulator a as an immediate value

* JMP jump to [PC+1] unconditionally

* JSR enters the subroutine, pushes PC+2 into the stack as the return address, and jumps to [PC+1]

* BZ: branch when the accumulator is zero, jump to [PC+1] when the accumulator a is 0, jump to PC+2 to continue execution when it is not zero

* BNZ: branch when the accumulator is not zero, jump to [PC+1] when the accumulator a is not 0, jump to PC+2 when it is zero

* ENT enters the subroutine, pushes bp on the stack, the base address bp points to the top of the stack, and then grows the word [PC+1] on the top of the stack, passing the space as a parameter

* LEV leaves subroutine, stack pointer sp = bp, pops base address bp, pc from stack

* LI takes a as the address to take an int number

* LC takes a as the address to take char

* SI uses the top of the stack as the address to store the int number and pop the stack [[sp++]]=a

* SC uses the top of the stack as the address to store char and pop the stack [[sp++]]=a

* PSH pushes a onto the stack

* OR a = [sp++] | a

* XOR a = [sp++]^a

* AND a = [sp++] & a

* EQ a = [sp++] == a

* NE a = [sp++] != a

* LT a = [sp++] a

* GT a = [sp++] > a

* LE a = [sp++] <= a

* GE a = [sp++] >= a

* SHL a = [sp++] << a

* SHR a = [sp++] >> a

* ADD a = [sp++] + a

* SUB a = [sp++] - a

* MUL a = [sp++] * a

* DIV a = [sp++] / a

* MOD a = [sp++] % a

* OPEN calls the C library function open, and the stack passes 2 parameters (the first parameter is advanced to the stack, and the return value is stored in the accumulator a, the same below)

* READ calls the C library function read, and the stack passes 2 parameters

* CLOS calls the C library function close, and the stack passes 2 parameters

* PRTF calls the C library function printf, [pc+1] indicates the number of parameters, and passes up to six parameters

* MALC calls the C library function malloc, passing a parameter on the stack

* MSET calls the C library function memset, and the stack passes 3 parameters

* MCMP calls the C library function memcmp, and the stack passes 3 parameters

* EXIT prints the execution of the virtual machine and returns [sp]
