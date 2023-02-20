# brainfuckc
## About

[Brainfuck](https://esolangs.org/wiki/Brainfuck) compiler/interpreter (bf) written in C.

This program can either be used as a compiler which compiles the bf code to C code or as an interpreter which interprets/runs the bf code right after parsing it.

## Installation

Simply compile the `bf.c` file or use the provided `Makefile`

```
git clone https://github.com/fymue/brainfuckc.git
cd brainfuckc
make
```

## Usage

```
Usage: ./bf [options] file.bf

Options:
-p, --parse  STR      parse STR as brainfuck code
-n, --tapesize N      specify the size of the tape (default: 30000)
-c, --compile         compile the brainfuck code to C code
```

### Interpreter mode

The input bf code can either be provided as a string argument with the `--parse` option or as a `.bf` file:

```
./bf --parse [>+.]
./bf path/to/bf_code.bf
```

### Compiler mode

To use `brainfuckc` as a compiler, simply add the `--compile` option:

```
./bf --compile --parse [>+.]
./bf --compile path/to/bf_code.bf
```

This will output a `bf_code.c` file (or whatever you named your `.bf` file + `.c`) which you can compile and execute yourself.

## TODO

As of right now, I haven't finished implementing the actual interpreter yet. In the meantime, the program will simply compile the bf code to C code and compile/execute that code when invoked in interpreter mode.