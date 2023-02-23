#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define PATH_LIMIT       100  // limit input path length for safety reasons
#define BF_FILE_EXT      ".bf"
#define INTERMEDIATE_EXT ".c"

// brainfuck commands
#define NEXT       '>'
#define PREV       '<'
#define INC        '+'
#define DEC        '-'
#define PRINT      '.'
#define INSERT     ','
#define START_LOOP '['
#define CLOSE_LOOP ']'

// resize array/ptr to n_bytes bytes
void* __resize(void *arr, size_t n_bytes) {
  assert(arr);
  void *new = NULL;
  new = realloc(arr, n_bytes);

  if (!new) {
    printf("Reallocation failed. Aborting...\n");
    abort();
  }

  return new;
}

// io-related functions

// check if input file ends in ".bf"
bool __is_bf_file(const char *file) {
  assert(file);
  size_t offset = strnlen(file, PATH_LIMIT) - strlen(BF_FILE_EXT);
  return strncmp(file + offset, BF_FILE_EXT, strlen(BF_FILE_EXT)) == 0;
}

// initialize the intermediate C file
int __init_out_file(FILE *out, int tape_size) {
  if (!out) {
    return 1;
  }

  fprintf(out,
         "#include <stdio.h>\n\n"
         "int main() {\n"
         "  unsigned char tape[%d] = {0};\n"
         "  unsigned char *i = tape;\n",
         tape_size);

  return 0;
}

// close the intermediate C file
int __close_out_file(FILE *out, int *indent) {
  *indent -= 2;
  fprintf(out, "}\n");

  return fclose(out);
}

// extract the ".bf" file name from the user-specified path
char* __get_file_name(const char *bf_file_path) {
  assert(bf_file_path);

  // find start position of filename
  const int PATH_LENGTH = strnlen(bf_file_path, PATH_LIMIT);
  int start = 0;
  for (int i = PATH_LENGTH; i-- > 0;) {
    if (bf_file_path[i] == '/') {
      start = i + 1;
      break;
    }
  }

  const int CURR_FILE_NAME_LENGTH = PATH_LENGTH - start;
  const int INTERMEDIATE_FILE_NAME_LENGTH = CURR_FILE_NAME_LENGTH -
                                            strlen(BF_FILE_EXT) +
                                            strlen(INTERMEDIATE_EXT);

  char *bf_file_name = (char*) malloc(INTERMEDIATE_FILE_NAME_LENGTH *
                                      sizeof(char) + 1);

  // copy filename minus ".bf" into new buffer
  memcpy(bf_file_name, bf_file_path + start,
         CURR_FILE_NAME_LENGTH - strlen(BF_FILE_EXT));

  // add ".c" extension and null-terminate the string
  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH - 2] = '.';
  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH - 1] = 'c';
  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH] = '\0';

  return bf_file_name;
}

long __get_file_size(FILE *file) {
  // get the size of file in bytes
  assert(file);

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  return size;
}

// parse the current character and add it
// to the code array if it is a bf command;
// also store loop opening addresses, which will later
// be needed to do conditional jumps in the interpreter function
void __parse_char(char c, unsigned char *code, long *idx,
                  int *open_loop_i, int *close_loop_i, int *mx_loops,
                  unsigned char ***open_loops) {
  switch (c) {
    case NEXT :
      code[(*idx)++] = NEXT;
      break;
    case PREV :
      code[(*idx)++] = PREV;
      break;
    case INC :
      code[(*idx)++] = INC;
      break;
    case DEC :
      code[(*idx)++] = DEC;
      break;
    case PRINT :
      code[(*idx)++] = PRINT;
      break;
    case INSERT :
      code[(*idx)++] = INSERT;
      break;
    case START_LOOP :
    {
      // add idx/address of START_LOOP instruction to array
      // and resize array if necessary

      if (*open_loop_i == *mx_loops) {
        *mx_loops *= 2;
        *open_loops = __resize(*open_loops,
                               *mx_loops * sizeof(unsigned char*));
      }

      unsigned char *current_addr = code + (*idx);
      (*open_loops)[(*open_loop_i)++] = current_addr;

      code[(*idx)++] = START_LOOP;
      assert(*current_addr == START_LOOP);
      break;
    }
    case CLOSE_LOOP :
      code[(*idx)++] = CLOSE_LOOP;
      (*close_loop_i)++;
      break;
  }
}

// execute the bf instructions
void __execute_instructions(unsigned char *code_ptr, const unsigned char *end,
                            unsigned char *tape_ptr, unsigned char **open_loops,
                            unsigned char **close_loops, int total_loops) {
  char curr_bf_instruction;
  int loop_i = -1, stack_ptr = -1;
  int stack[total_loops];  // keep track of loop nesting

  // parse the current bf instruction and execute
  // the appropriate C instruction
  while (code_ptr < end) {
    curr_bf_instruction = *code_ptr;
    printf(" %c ", curr_bf_instruction);

    switch (curr_bf_instruction) {
      case NEXT :
        ++tape_ptr;
        break;
      case PREV :
        --tape_ptr;
        break;
      case INC :
        (*tape_ptr)++;
        break;
      case DEC :
        (*tape_ptr)--;
        break;
      case PRINT :
        putchar(*tape_ptr);
        break;
      case INSERT :
        *tape_ptr = getchar();
        break;
      case START_LOOP :
        // <<[++[--]<<[--]]++[<]

        // if a loop is opened, jump to the end of the loop
        // and evaluate the loop condition;
        // if it's true, jump back to this address + 1;
        // if it's false, simply keep going
        ++loop_i;
        ++stack_ptr;
        stack[stack_ptr] = loop_i;
        printf("stack open = %d\n", stack[stack_ptr]);
        code_ptr = close_loops[stack[stack_ptr]];
        assert(*code_ptr == CLOSE_LOOP);
        continue;  // don't increment the code ptr
      case CLOSE_LOOP :
        if (*tape_ptr != 0) {  // bf loop condition
          // jump back to loop opening
          printf("stack = %d\n", stack[stack_ptr]);
          code_ptr = open_loops[stack[stack_ptr]];
          assert(*code_ptr == START_LOOP);
        } else {
          --stack_ptr;
        }
    }

    ++code_ptr;
  }
}

// parse the current character and print
// its C equivalent instruction to the output file
// (with proper identation)
void __generate_instruction(FILE *out, char c, int *indent) {
  switch (c) {
    case NEXT :
      fprintf(out, "%*s\n", (int)(strlen("++i;") + *indent), "++i;");
      break;
    case PREV :
      fprintf(out, "%*s\n", (int)(strlen("--i;") + *indent), "--i;");
      break;
    case INC :
      fprintf(out, "%*s\n", (int)(strlen("(*i)++;") + *indent), "(*i)++;");
      break;
    case DEC :
      fprintf(out, "%*s\n", (int)(strlen("(*i)--;") + *indent), "(*i)--;");
      break;
    case PRINT :
      fprintf(out, "%*s\n", (int)(strlen("putchar(*i);") + *indent),
                            "putchar(*i);");
      break;
    case INSERT :
      fprintf(out, "%*s\n", (int)(strlen("*i = getchar();") + *indent),
                            "*i = getchar();");
      break;
    case START_LOOP :
      fprintf(out, "%*s\n", (int)(strlen("while (*i != 0) {") + *indent),
                            "while (*i != 0) {");
      *indent += 2;
      break;
    case CLOSE_LOOP :
      *indent -= 2;
      fprintf(out, "%*s\n", (int)strlen("}") + *indent, "}");
      break;
  }
}

// iterate over all loop opening addresses
// and find their corresponding closing addresses
void __find_loop_closing_addresses(unsigned char **open_loops,
                                   int total_loops,
                                   unsigned char **close_loops) {
  unsigned char curr_instruction;

  for (int i = 0; i < total_loops; i++) {
    unsigned char *curr_loop_ptr = open_loops[i] + 1;
    int loop_c = 1;

    while (1) {
      curr_instruction = *curr_loop_ptr;

      if (curr_instruction == START_LOOP) {
        ++loop_c;
      } else if (curr_instruction == CLOSE_LOOP) {
        --loop_c;
        if (loop_c == 0) {
          assert(*curr_loop_ptr == CLOSE_LOOP);
          close_loops[i] = curr_loop_ptr;
          break;
        }
      }

      ++curr_loop_ptr;
    }
  }
}

// execute the generated C code
void execute_generated_code(const char *file_name) {
  const int COMMAND_BUFFER_SIZE = 100;
  char cmd[COMMAND_BUFFER_SIZE];

  snprintf(cmd, COMMAND_BUFFER_SIZE,
           "gcc -O3 -o tmp %s && ./tmp && rm -f tmp",
           file_name);
  system(cmd);
}

// interpret and run the bf code
int run_bf_code(bool parse_str, const char *bf_string,
                const char *input_file, int tape_size) {
  /*
     the bf code will first be parsed and
     all valid bf instructions will be written
     to an array (so the interpreter can jump around);
     additionally, loop start/close addresses will be stored
     so the interpreter knows where to jump when a loop condition
     is true/false;
     after all preprocessing is complete, the interpreter is called
  */

  size_t size = 0;
  FILE *bf_file = NULL;
  if (parse_str) {
    size = strlen(bf_string);
  } else {
    bf_file = fopen(input_file, "r");
    size = __get_file_size(bf_file);
  }

  unsigned char* code = (unsigned char*) malloc(size);

  long idx = 0;
  int open_loop_i = 0, close_loop_i = 0, mx_loops = 10, fail = 0;

  // stores addresses where loops start
  unsigned char **open_loops = (unsigned char**)
                                malloc(mx_loops * sizeof(unsigned char*));

  if (parse_str) {
    // read all bf instructions from the input string
    // and write them to the code array
    for (size_t i = 0; i < size; i++) {
      __parse_char(bf_string[i], code, &idx, &open_loop_i, &close_loop_i,
                   &mx_loops, &open_loops);
    }
  } else {
    char c;

    if (!bf_file) {
      printf("%s can't be opened.\n", input_file);
      return 1;
    }
    do {
      c = fgetc(bf_file);
      __parse_char(c, code, &idx, &open_loop_i,
                   &close_loop_i, &mx_loops, &open_loops);
    } while (c != EOF);
    fclose(bf_file);
  }

  if (open_loop_i > close_loop_i) {
    printf("More open loop instructions ('[') than "
           "close loop instructions (']').\n");
    fail = 1;
  } else if (open_loop_i < close_loop_i) {
    printf("Less loop instructions ('[') than "
           "close loop instructions (']').\n");
    fail = 1;
  }

  if (!fail) {
    // store addresses where loops are closed
    unsigned char **close_loops = (unsigned char**)
                                  malloc(open_loop_i * sizeof(unsigned char*));

    // find corresponding closing address for every loop opening address
    __find_loop_closing_addresses(open_loops, open_loop_i, close_loops);

    // zero-initialize the tape (default size: 30000)
    unsigned char *tape = (unsigned char*) calloc(tape_size, sizeof(char));
    const unsigned char *end = code + size;

    // run the actual interpreter
    __execute_instructions(code, end, tape, open_loops,
                           close_loops, open_loop_i);

    free(tape);
    free(close_loops);
  }

  free(code);
  free(open_loops);

  return fail;
}

// parse the provided string as bf code
int parse_bf_string(const char *bf_string, int tape_size,
                    bool compile) {
  if (!bf_string) {
    return 1;
  }
  const size_t n_chars = strlen(bf_string);
  int fail = 0;
  //           vvvvvv    temporary until actual interpreter works
  if (compile || true) {
    // create intermediate C code
    const char *file_name = "bf_program.c";
    FILE *out = fopen(file_name, "w");

    fail = __init_out_file(out, tape_size);
    if (fail) return fail;

    int indent = 2;

    // loop over input string and parse every character/instruction
    for (size_t i = 0; i < n_chars; i++) {
      __generate_instruction(out, bf_string[i], &indent);
    }

    fail = __close_out_file(out, &indent);

    if (fail) return fail;

    execute_generated_code(file_name);
  } else {
    // run interpreter
    fail = run_bf_code(true, bf_string, NULL, tape_size);
  }

  return fail;
}

// parse the ".bf" file
int parse_bf_file(const char *bf_file_path, int tape_size,
                  bool compile) {
  int fail = 0;
  if (!bf_file_path) {
    fail = 1;
    return fail;
  }

  //           vvvvvv    temporary until actual interpreter works
  if (compile || true) {
    // generate intermediate C code

    FILE *bf_file = fopen(bf_file_path, "r");
    if (!bf_file) {
      printf("%s can't be opened.\n", bf_file_path);
      return 1;
    }

    char *file_name = __get_file_name(bf_file_path);
    FILE *out = fopen(file_name, "w");

    fail = __init_out_file(out, tape_size);
    if (fail) return fail;

    int indent = 2;
    char c;

    // parse every character of the input file
    do {
      c = fgetc(bf_file);
      __generate_instruction(out, c, &indent);
    } while (c != EOF);

    fail = __close_out_file(out, &indent);
    if (fail) return fail;

    fail = fclose(bf_file);
    if (fail) return fail;

    execute_generated_code(file_name);

    free(file_name);
  } else {
    // run interpreter
    run_bf_code(false, NULL, bf_file_path, tape_size);
  }

  return fail;
}

// cmd-line functions

// print usage help
void help() {
  printf("Usage: ./bf [options] file.bf\n\n"
         "Options:\n"
         "-p, --parse  STR      parse STR as brainfuck code\n"
         "-n, --tapesize N      specify the size of the tape (default: 30000)\n"
         "-c, --compile         compile brainfuck code to C code\n\n");
}

// parse the command line arguments and call the
// appropriate backend functions
int parse_args(int argc, char **argv) {
  if (argc == 1) {
    help();
    return 0;
  }

  char *bf_file_path = NULL, *bf_str = NULL;
  int tape_size = 30000;  // default tape size
  bool compile = false;  // run as interpreter by default

  const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"compile", no_argument, NULL, 'c'},
    {"parse", required_argument, NULL, 'p'},
    {"tapesize", required_argument, NULL, 'n'},
    {NULL, no_argument, NULL, 0}};

  int o = 0;

  while ((o = getopt_long(argc, argv, ":hcn:p:", long_opts, NULL)) != -1) {
    switch (o) {
      case 'h' :
        help();
        return 0;
      case 'c' :
        compile = true;
        break;
      case 'p' :
        bf_str = optarg;
        break;
      case 'n' :
        tape_size = atoi(optarg);
        break;
      case '?' :
      case ':' :
      default :
        printf("Unkown option -%c\n", (char)optopt);
        return 1;
    }
  }

  if (!bf_str) {
    // only check for a valid input file if
    // no input string was provided
    bool is_bf_file = __is_bf_file(argv[argc - 1]);
    if (optind == argc || !is_bf_file) {
      printf("No .bf file provided."
             "Please provide a file that ends in .bf\n"
             "and ensure that the path length doesn't exceed %d characters.\n",
             PATH_LIMIT);
      return 1;
    }

    bf_file_path = argv[argc - 1];
    return parse_bf_file(bf_file_path, tape_size, compile);
  } else {
    return parse_bf_string(bf_str, tape_size, compile);
  }
}

int main(int argc, char **argv) {
  int fail = parse_args(argc, argv);
  return fail;
}
