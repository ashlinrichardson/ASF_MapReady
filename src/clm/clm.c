#define ASF_NAME_STRING "clm"

#define ASF_USAGE_STRING \
"   "ASF_NAME_STRING" <function name> <.meta file> <function arguments>\n"

#define ASF_DESCRIPTION_STRING \
"     This program calls the given metadata function with the given arguments\n"\
"     and prints the result.\n"

#define ASF_INPUT_STRING \
"     <function name>:\n"\
"     Name of an ASF metadata function, (e.g., meta_get_slant)\n"\
"     Only certain metadata function are supported.  See the list below.\n"\
"\n"\
"     <.meta file>:\n"\
"     Name of a .meta file, for which the specified function should be called.\n"\
"\n"\
"     <function arguments>\n"\
"     Varies with the function specified.  See below.  Typically will be a\n"\
"     line number and a sample number.  Special values:\n"\
"       Center line: CL\n"\
"       Center sample: CS\n"\
"       End line (sample): EL (ES)\n"\

#define ASF_OUTPUT_STRING \
"     No output files are produced, the result of the function is printed to\n"\
"     stdout.\n"

#define ASF_OPTIONS_STRING \
"     -log <log file>\n"\
"          Output will be written to a specified log file.\n"\
"\n"\
"     -quiet\n"\
"          Supresses all non-essential output.\n"\
"\n"\
"     -license\n"\
"          Print copyright and license for this software then exit.\n"\
"\n"\
"     -version\n"\
"          Print version and copyright then exit.\n"\
"\n"\
"     -help\n"\
"          Print a help page and exit.\n"

#define ASF_EXAMPLES_STRING \
"       > "ASF_NAME_STRING" meta_get_slant FILE.meta 1000 1000\n"\
"       > "ASF_NAME_STRING" meta_get_slant FILE.meta CL CS\n"\

#include <stdio.h>
#include <asf.h>
#include <asf_meta.h>
#include <asf_raster.h>
#include <asf_license.h>
#include <asf_contact.h>

static void use(const char *msg)
{
  asfPrintStatus("%s\n", msg);
  exit(1);
}

void clm(const char *func, meta_parameters *meta, double *args, int nArgs)
{
  if (strcmp_case(func, "meta_get_slant") == 0) {
    if (nArgs != 2) use("meta_get_slant: <line> <sample>\n");
    asfPrintStatus("%g\n", meta_get_slant(meta, args[0], args[1]));
  }
  else {
    use("Unknown: %s\n");
  }
}

// Print minimalistic usage info & exit
static void usage(const char *name)
{
  asfPrintStatus("\n"
      "Usage:\n"
      ASF_USAGE_STRING
      "\n");
  exit(EXIT_FAILURE);
}

// Print the help info & exit
static void print_help(void)
{
  asfPrintStatus(
      "\n"
      "Tool name:\n   " ASF_NAME_STRING "\n\n"
      "Usage:\n" ASF_USAGE_STRING "\n"
      "Description:\n" ASF_DESCRIPTION_STRING "\n"
      "Input:\n" ASF_INPUT_STRING "\n"
      "Output:\n"ASF_OUTPUT_STRING "\n"
      "Options:\n" ASF_OPTIONS_STRING "\n"
      "Examples:\n" ASF_EXAMPLES_STRING "\n"
      "Contact:\n" ASF_CONTACT_STRING "\n"
      "Version:\n"  TOOL_SUITE_NAME " " TOOL_SUITE_VERSION_STRING "\n\n");
  exit(EXIT_SUCCESS);
}

int strmatches(const char *key, ...)
{
    va_list ap;
    char *arg = NULL;
    int found = FALSE;

    va_start(ap, key);
    do {
        arg = va_arg(ap, char *);
        if (arg) {
            if (strcmp(key, arg) == 0) {
                found = TRUE;
                break;
            }
        }
    } while (arg);

    return found;
}

// Main program body.
int
main (int argc, char *argv[])
{
  char *meta_file, *func;
  int currArg = 1;
  int NUM_ARGS = 2;

  handle_license_and_version_args(argc, argv, ASF_NAME_STRING);

  if (argc<=1)
      usage(ASF_NAME_STRING);
  else if (strmatches(argv[1],"-help","--help",NULL))
      print_help();

  while (currArg < (argc-NUM_ARGS)) {
    char *key = argv[currArg++];
    if (strmatches(key,"-help","--help",NULL)) {
        print_help(); // doesn't return
    }
    else if (strmatches(key,"-log","--log",NULL)) {
      CHECK_ARG(1);
      strcpy(logFile,GET_ARG(1));
      fLog = FOPEN(logFile, "a");
      logflag = TRUE;
    }
    else if (strmatches(key,"-quiet","--quiet","-q",NULL)) {
      quietflag = TRUE;
    }
    else {
        --currArg;
        break;
    }
  }

  if ((argc-currArg) < NUM_ARGS) {
    printf("Insufficient arguments.\n");
    usage(argv[0]);
  }

  int nArgs = argc - (currArg+2);
  func = argv[currArg];
  meta_file = argv[currArg+1];

  double *args = MALLOC(sizeof(double)*nArgs);
  meta_parameters *meta = meta_read(meta_file);

  int ii,jj;
  for (jj=0, ii=currArg+2; ii<argc; ++ii, ++jj) {
    if (strcmp_case(argv[ii], "CL") == 0) {
      args[jj] = meta->general->line_count/2.0;
    } else if (strcmp_case(argv[ii], "CS") == 0) {
      args[jj] = meta->general->sample_count/2.0;
    } else if (strcmp_case(argv[ii], "EL") == 0) {
      args[jj] = meta->general->line_count;
    } else if (strcmp_case(argv[ii], "ES") == 0) {
      args[jj] = meta->general->sample_count;
    } else {
      args[jj] = atof(argv[ii]);
    }
  }

  clm(func, meta, args, nArgs);

  FREE(args);
  meta_free(meta);
  return EXIT_SUCCESS;
}

