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
         "-p, --parse STR       parse STR as brainfuck code\n"
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
               char **bf_file, char **input_bf_str) {
  // parse the command line arguments
  if (argc == 1) {
    help();
    return 0;
  }

  const struct option long_opts[] = {
    {"help", no_argument, NULL, 'h'},
    {"intermediate", no_argument, NULL, 'i'},
    {"parse", required_argument, NULL, 'p'},
    {NULL, no_argument, NULL, 0}};

  int o = 0;

  while ((o = getopt_long(argc, argv, ":hi", long_opts, NULL)) != -1) {
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

void __parse_char(FILE *out, char c) {
  // parse the current character
  switch (c) {
    case NEXT :
      break;
    case PREV :
      break;
    case INC :
      break;
    case DEC :
      break;
    case PRINT :
      break;
    case INSERT :
      break;
    case START_LOOP :
      break;
    case CLOSE_LOOP :
      break;
  }
}

int parse_bf_string(const char *bf_string) {
  // parse the provided string as if it was a ".bf" file
  // and create intermediate C code from it

  if (!bf_string) {
    return 1;
  }

  const size_t n_chars = strlen(bf_string);
  for (size_t i = 0; i < n_chars; i++) {
    // FIXME: __parse_char(..., bf_string[i]);
  }

  return 0;
}

int parse_bf_file(const char *bf_file_path) {
  // parse the ".bf" file and create intermediate C code from it

  if (!bf_file_path) {
    return 1;
  }

  FILE *bf_file = fopen(bf_file_path, "r");

  if (!bf_file) {
    printf("%s can't be openend.\n", bf_file_path);
    return 1;
  }

  char c;
  do {
    c = fgetc(bf_file);
    // FIXME: __parse_char(..., c);
  } while (c != EOF);

  fclose(bf_file);
  return 0;
}

int main(int argc, char **argv) {
  char *bf_file_path = NULL, *bf_str = NULL;

  int fail = parse_args(argc, argv, &bf_file_path, &bf_str);
  if (fail) {
    return 1;
  }

  if (bf_str) {
    fail = parse_bf_string(bf_str);
  } else {
    fail = parse_bf_file(bf_file_path);
  }

  if (fail) {
    return 1;
  }

  return 0;
}
