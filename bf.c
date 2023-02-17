#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

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

void help() {
  printf("Usage: ./bf [options] file.bf\n\n"
         "Options:\n"
         "-p, --parse  STR      parse STR as brainfuck code\n"
         "-n, --tapesize N      specify the size of the tape (default: 30000)\n"
         "-i, --intermediate    generate intermediate C code\n\n");
}

bool _is_bf_file(const char *file) {
  // check if input file ends in ".bf"
  size_t offset = strnlen(file, PATH_LIMIT) - 3;
  return strncmp(file + offset, BF_FILE_EXT, 3) == 0;
}

int __init_out_file(FILE *out, int tape_size) {
  // initialize the output intermediate C file
  // (either as a file or a stream depending on user input)

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

int __close_out_file(FILE *out, int *indent) {
  *indent -= 2;
  fprintf(out, "}\n");

  return fclose(out);
}

void __parse_char(FILE *out, char c, int *indent) {
  // parse the current character and print
  // its C equivalent instruction to the output file(stream)
  switch (c) {
    case NEXT :
      fprintf(out, "%*s\n", strlen("++i;") + *indent, "++i;");
      break;
    case PREV :
      fprintf(out, "%*s\n", strlen("--i;") + *indent, "--i;");
      break;
    case INC :
      fprintf(out, "%*s\n", strlen("(*i)++;") + *indent, "(*i)++;");
      break;
    case DEC :
      fprintf(out, "%*s\n", strlen("(*i)--;") + *indent, "(*i)--;");
      break;
    case PRINT :
      fprintf(out, "%*s\n", strlen("printf(\"%c\", *i);") + *indent,
                             "printf(\"%c\", *i);");
      break;
    case INSERT :
      fprintf(out, "%*s\n", strlen("*i = getchar();") + *indent,
                             "*i = getchar();");
      break;
    case START_LOOP :
      fprintf(out, "%*s\n", strlen("while (*i != 0) {") + *indent,
                             "while (*i != 0) {");
      *indent += 2;
      break;
    case CLOSE_LOOP :
      *indent -= 2;
      fprintf(out, "%*s\n", strlen("}") + *indent, "}");
      break;
  }
}

void __run_bf_code(const char *file_name) {
  // execute the generated C code
  const int COMMAND_BUFFER_SIZE = 100;
  char cmd[COMMAND_BUFFER_SIZE];

  snprintf(cmd, COMMAND_BUFFER_SIZE,
           "gcc -O3 -o tmp %s && ./tmp && rm -f tmp %s",
           file_name, file_name);
  system(cmd);
}

int parse_bf_string(const char *bf_string, int tape_size,
                    bool generate_intermediate) {
  // parse the provided string as if it was a ".bf" file
  // and create intermediate C code from it

  if (!bf_string) {
    return 1;
  }

  const char *file_name = "bf_program.c";
  FILE *out = fopen(file_name, "w");

  int fail = __init_out_file(out, tape_size);
  if (fail) return fail;

  int indent = 2;

  const size_t n_chars = strlen(bf_string);
  for (size_t i = 0; i < n_chars; i++) {
    __parse_char(out, bf_string[i], &indent);
  }

  fail = __close_out_file(out, &indent);
  if (fail) return fail;

  if (!generate_intermediate) {
    __run_bf_code(file_name);
  }

  return 0;
}

char* __get_file_name(const char *bf_file_path) {
  // extract the ".bf" file name from the user-specified path
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
  memcpy(bf_file_name, bf_file_path + start,
         CURR_FILE_NAME_LENGTH - strlen(BF_FILE_EXT));

  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH - 2] = '.';
  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH - 1] = 'c';
  bf_file_name[INTERMEDIATE_FILE_NAME_LENGTH] = '\0';

  return bf_file_name;
}

int parse_bf_file(const char *bf_file_path, int tape_size,
                  bool generate_intermediate) {
  // parse the ".bf" file and create intermediate C code from it

  if (!bf_file_path) {
    return 1;
  }

  FILE *bf_file = fopen(bf_file_path, "r");

  if (!bf_file) {
    printf("%s can't be openend.\n", bf_file_path);
    return 1;
  }

  char *file_name = NULL;
  char *tmp_name = "tmp_bf_program.c";
  int fail = 0;
  if (generate_intermediate) {
    file_name = __get_file_name(bf_file_path);
  } else {
    file_name = tmp_name;
  }
  FILE *out = fopen(file_name, "w");

  fail = __init_out_file(out, tape_size);
  if (fail) return fail;

  int indent = 2;
  char c;

  do {
    c = fgetc(bf_file);
    __parse_char(out, c, &indent);
  } while (c != EOF);

  fail = fclose(bf_file);
  if (fail) return fail;

  fail = __close_out_file(out, &indent);
  if (fail) return fail;

  if (!generate_intermediate) {
    __run_bf_code(file_name);
  } else {
    free(file_name);
  }

  return 0;
}

int parse_args(int argc, char **argv) {
  // parse the command line arguments
  // and call the "compiler backend" (if you want to call it that)
  if (argc == 1) {
    help();
    return 0;
  }

  char *bf_file_path = NULL, *bf_str = NULL;
  int tape_size = 30000;
  bool generate_intermediate = false;

  const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"intermediate", no_argument, NULL, 'i'},
    {"parse", required_argument, NULL, 'p'},
    {"tapesize", required_argument, NULL, 'n'},
    {NULL, no_argument, NULL, 0}};

  int o = 0;

  while ((o = getopt_long(argc, argv, ":hin:", long_opts, NULL)) != -1) {
    switch (o) {
      case 'h' :
        help();
        return 0;
      case 'i' :
        generate_intermediate = true;
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
    // only look for an input file if
    // no input string was provided
    bool is_bf_file = _is_bf_file(argv[argc - 1]);
    if (optind == argc || !is_bf_file) {
      printf("No .bf file provided."
             "Please provide a filethat ends in .bf\n"
             "and that the path length doesn't exceed %d characters.\n",
             PATH_LIMIT);
      return 1;
    }

    bf_file_path = argv[argc - 1];
    return parse_bf_file(bf_file_path, tape_size, generate_intermediate);
  } else {
    return parse_bf_string(bf_str, tape_size, generate_intermediate);
  }
}

int main(int argc, char **argv) {
  return parse_args(argc, argv);
}
