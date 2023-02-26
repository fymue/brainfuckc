#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>

#define PATH_LIMIT       100  // limit input path length for safety reasons
#define BF_FILE_EXT      ".bf"
#define INTERMEDIATE_EXT ".c"
#define MAX_TAPE_SIZE    1000000000  // limit tape size

#ifdef _WIN32
#define PATH_SEPARATOR   '\\'
#else
#define PATH_SEPARATOR   '/'
#endif

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
    fprintf(stderr, "Reallocation failed. Aborting...\n");
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
         "  unsigned char *i = tape;\n"
         "  char c;\n",
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
    if (bf_file_path[i] == PATH_SEPARATOR) {
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

/*
   parse the current character and add it
   to the code array if it is a bf command;
   also store loop opening addresses, which will later
   be needed to do conditional jumps in the interpreter
*/
long __parse_char(char c, unsigned char *code, long idx,
                  int *open_loop_i, int *close_loop_i,
                  int *mx_loops, long *open_loops) {
  switch (c) {
    case NEXT :
      code[idx] = NEXT;
      break;
    case PREV :
      code[idx] = PREV;
      break;
    case INC :
      code[idx] = INC;
      break;
    case DEC :
      code[idx] = DEC;
      break;
    case PRINT :
      code[idx] = PRINT;
      break;
    case INSERT :
      code[idx] = INSERT;
      break;
    case START_LOOP :
      code[idx] = START_LOOP;

      // add idx of START_LOOP instruction to array
      // and resize array if necessary
      if (*open_loop_i == *mx_loops) {
        *mx_loops *= 2;
        open_loops = __resize(open_loops, (*mx_loops) * sizeof(long));
      }

      open_loops[(*open_loop_i)++] = idx;
      break;
    case CLOSE_LOOP :
      code[idx] = CLOSE_LOOP;
      (*close_loop_i)++;
      break;
  }

  return ++idx;
}

// execute the bf instructions
void __execute_instructions(unsigned char *code_ptr, const unsigned char *end,
                            unsigned char *tape_ptr, unsigned char **loops) {
  char curr_bf_instruction, inp;
  unsigned char *start = code_ptr;
  long count = 0;

  // parse the current bf instruction and execute
  // the appropriate C instruction
  while (code_ptr < end) {
    curr_bf_instruction = *code_ptr;

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
        if ((inp = getchar()) != EOF) *tape_ptr = inp;
        break;
      case START_LOOP :
        // if a loop is opened, jump to the end of the loop
        // and evaluate the loop condition;
        // if it's true, jump back to this address + 1;
        // if it's false, simply keep going
        ptrdiff_t close_offset = code_ptr - start;
        assert(close_offset >= 0);
        code_ptr = loops[close_offset];

        assert(code_ptr);
        assert(*code_ptr == CLOSE_LOOP);
        continue;
      case CLOSE_LOOP :
        if (*tape_ptr != 0) {  // bf loop condition
          // jump back to loop opening
          ptrdiff_t open_offset = code_ptr - start;
          assert(open_offset > 0);
          code_ptr = loops[open_offset] + 1;

          assert(code_ptr);
          assert(*(code_ptr-1) == START_LOOP);
          continue;
        }
    }

    ++code_ptr;
    count++;
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
      fprintf(out, "%*s\n",
              (int)(strlen("if ((c = getchar()) != EOF) *i = c;") + *indent),
              "if ((c = getchar()) != EOF) *i = c;");
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

/*
   iterate over all loop opening addresses
   and find their corresponding closing addresses;
   addresses will be written into the "loops" array,
   which serves as a "key-value" storage that returns
   the closing loop address for every opening loop
   address and vice versa
*/
int __get_loop_addresses(long *open_loops, int total_loops,
                         unsigned char *code, long size,
                         unsigned char **loops) {
  unsigned char curr_instruction;

  for (int i = 0; i < total_loops; i++) {
    // start iterating from 1st instruction after START_LOOP instruction
    long curr_open_loop_start = open_loops[i];

    int loop_c = 1;  // count how often a new loop is opened/closed

    for (long i = curr_open_loop_start + 1; i < size; i++) {
      curr_instruction = code[i];

      if (curr_instruction == START_LOOP) {
        ++loop_c;
      } else if (curr_instruction == CLOSE_LOOP) {
        --loop_c;

        if (loop_c == 0) {
          // add CLOSE_LOOP address to idx where START_LOOP instruction sits,
          // so the interpreter can directly jump there
          // (and do the opposite for the START_LOOP case)
          long offset_to_closing_loop = i;
          long offset_to_opening_loop = curr_open_loop_start;

          loops[curr_open_loop_start] = code + offset_to_closing_loop;
          loops[i] = code + offset_to_opening_loop;
          break;
        }
      }
    }

    if (loop_c != 0) return 1;
  }

  return 0;
}

// execute the generated C code
void execute_generated_code(const char *file_name, bool remove) {
  const int COMMAND_BUFFER_SIZE = 100;
  char cmd[COMMAND_BUFFER_SIZE];

  if (remove) {
    snprintf(cmd, COMMAND_BUFFER_SIZE,
             "gcc -O3 -o tmp %s && ./tmp && rm -f tmp %s",
             file_name, file_name);
  } else {
    snprintf(cmd, COMMAND_BUFFER_SIZE,
             "gcc -O3 -o tmp %s && ./tmp && rm -f tmp",
             file_name);
  }

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

  unsigned char *code = (unsigned char*) malloc(size);
  unsigned char **loops = (unsigned char**) malloc(size *
                                                  sizeof(unsigned char*));

  long idx = 0;
  int open_loop_i = 0, close_loop_i = 0;  // counters for opening/closing loops
  int mx_loops = 10, fail = 0;

  // stores indices where loops start
  long *open_loops = (long*) malloc(mx_loops * sizeof(long));

  if (parse_str) {
    // read all bf instructions from the input string
    // and write them to the code array
    for (size_t i = 0; i < size; i++) {
      idx = __parse_char(bf_string[i], code, idx, &open_loop_i,
                         &close_loop_i, &mx_loops, open_loops);
    }
  } else {
    // read all bf instructions from the input file
    // and write them to the code array
    char c;

    if (!bf_file) {
      fprintf(stderr, "%s can't be opened.\n", input_file);
      return 1;
    }

    do {
      c = fgetc(bf_file);
      idx = __parse_char(c, code, idx, &open_loop_i,
                         &close_loop_i, &mx_loops, open_loops);
    } while (c != EOF);

    fclose(bf_file);
  }

  if (open_loop_i > close_loop_i) {
    fprintf(stderr, "More open loop instructions ('[') than "
           "close loop instructions (']').\n");
    fail = 1;
  } else if (open_loop_i < close_loop_i) {
    fprintf(stderr, "Less loop instructions ('[') than "
           "close loop instructions (']').\n");
    fail = 1;
  }

  if (!fail) {
    // find corresponding closing address for every loop opening address
    fail = __get_loop_addresses(open_loops, open_loop_i, code, size, loops);
    free(open_loops);

    if (fail) return fail;

    // zero-initialize the tape (default size: 30000)
    unsigned char *tape = (unsigned char*) calloc(tape_size, sizeof(char));
    const unsigned char* end = code + size;  // marks the end of the bf code

    // run the actual interpreter
    __execute_instructions(code, end, tape, loops);

    free(tape);
  }

  free(code);
  free(loops);

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

  if (compile) {
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

  if (compile) {
    // generate intermediate C code

    FILE *bf_file = fopen(bf_file_path, "r");
    if (!bf_file) {
      fprintf(stderr, "%s can't be opened.\n", bf_file_path);
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

    free(file_name);
  } else {
    // run interpreter
    fail = run_bf_code(false, NULL, bf_file_path, tape_size);
  }

  return fail;
}

// cmd-line functions

// print usage help
void help() {
  fprintf(stderr, "Usage: ./bf [options] file.bf\n\n"
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
        if (tape_size > MAX_TAPE_SIZE) {
          fprintf(stderr, "Specified tape size is too large "
                 "(max is set to 1B cells or 1GB RAM).\n"
                 "Please increase MAX_TAPE_SIZE if you "
                 "need more than 1B cells.\n");
          return 1;
        }
        break;
      case '?' :
      case ':' :
      default :
        fprintf(stderr, "Unkown option -%c\n", (char)optopt);
        return 1;
    }
  }

  if (!bf_str) {
    // only check for a valid input file if
    // no input string was provided
    bool is_bf_file = __is_bf_file(argv[argc - 1]);
    if (optind == argc || !is_bf_file) {
      fprintf(stderr, "No .bf file provided.\n"
             "Please provide a file that ends in \".bf\"\n"
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
