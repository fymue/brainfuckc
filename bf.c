#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

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

void generate_intermediate() {
}

bool _is_bf_file(const char *file) {
  // check if input file ends in ".bf"
  size_t offset = strlen(file) - 3;
  return strncmp(file + offset, ".bf", 3) == 0;
}

int parse_args(int argc, char **argv,
               char **bf_file, char **input_bf_str,
               int *tape_size) {
  // parse the command line arguments
  if (argc == 1) {
    help();
    return 0;
  }

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
        break;
      case 'i' :
        generate_intermediate();
        break;
      case 'p' :
        *input_bf_str = optarg;
        break;
      case 'n' :
        *tape_size = atoi(optarg);
        break;
      case '?' :
      default :
        printf("Unkown option -%c\n", (char)optopt);
        return 1;
    }
  }

  if (!!input_bf_str) {
    // only look for an input file if
    // no input string was provided
    bool is_bf_file = _is_bf_file(argv[argc - 1]);
    if (optind == argc || !is_bf_file) {
      printf("No .bf file provided.\n");
      return 1;
    }

    *bf_file = argv[argc - 1];
  }

  return 0;
}

FILE* __init_out_file(int tape_size) {
  // initialize the output intermediate C file
  // (either as a file or a stream depending on user input)
  FILE *out = fopen("parsed_bf.c", "w");

  if (!out) {
    return NULL;
  }

  fprintf(out,
         "#include <stdio.h>\n\n"
         "int main() {\n"
         "  unsigned char tape[%d] = {0};\n"
         "  unsigned char *i = tape;\n",
         tape_size);

  return out;
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

int parse_bf_string(const char *bf_string, int tape_size) {
  // parse the provided string as if it was a ".bf" file
  // and create intermediate C code from it

  if (!bf_string) {
    return 1;
  }

  FILE *out = __init_out_file(tape_size);
  int indent = 2;

  const size_t n_chars = strlen(bf_string);
  for (size_t i = 0; i < n_chars; i++) {
    __parse_char(out, bf_string[i], &indent);
  }

  return __close_out_file(out, &indent);
}

int parse_bf_file(const char *bf_file_path, int tape_size) {
  // parse the ".bf" file and create intermediate C code from it

  if (!bf_file_path) {
    return 1;
  }

  FILE *bf_file = fopen(bf_file_path, "r");

  if (!bf_file) {
    printf("%s can't be openend.\n", bf_file_path);
    return 1;
  }

  FILE *out = __init_out_file(tape_size);
  int indent = 2;

  char c;
  do {
    c = fgetc(bf_file);
    __parse_char(out, c, &indent);
  } while (c != EOF);

  int fail = fclose(bf_file);
  if (fail) {
    return 1;
  }

  return __close_out_file(out, &indent);
}

int main(int argc, char **argv) {
  char *bf_file_path = NULL, *bf_str = NULL;
  int tape_size = 30000;

  int fail = parse_args(argc, argv, &bf_file_path,
                        &bf_str, &tape_size);
  if (fail) {
    return 1;
  }

  if (bf_str) {
    fail = parse_bf_string(bf_str, tape_size);
  } else {
    fail = parse_bf_file(bf_file_path, tape_size);
  }

  if (fail) {
    return 1;
  }

  return 0;
}
