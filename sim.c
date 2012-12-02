/*	This file is part of the software similarity tester SIM.
	Written by Dick Grune, Vrije Universiteit, Amsterdam.
	$Id: sim.c,v 2.29 2012-06-05 14:58:39 Gebruiker Exp $
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"system.par"
#include	"settings.par"
#include	"sim.h"
#include	"options.h"
#include	"newargs.h"
#include	"language.h"
#include	"error.h"
#include	"text.h"
#include	"runs.h"
#include	"hash.h"
#include	"compare.h"
#include	"pass1.h"
#include	"pass2.h"
#include	"pass3.h"
#include	"percentages.h"
#include	"stream.h"
#include	"lex.h"
#include	"Malloc.h"

/* command-line parameters */
unsigned int Min_Run_Size = DEFAULT_MIN_RUN_SIZE;
int Page_Width = DEFAULT_PAGE_WIDTH;
int Threshold_Percentage = 1;		/* minimum percentage to show */
FILE *Output_File;
FILE *Debug_File;

/* and their string values, for language files that define their own parameters
*/
const char *token_name = "token";
const char *min_run_string;
const char *threshold_string;

const char *progname;			/* for error reporting */

static const char *page_width_string;
static const char *output_name;		/* for reporting */

static const struct option optlist[] = {
	{'r', "minimum run size", 'N', &min_run_string},
	{'w', "page width", 'N', &page_width_string},
	{'f', "function-like forms only", ' ', 0},
	{'F', "keep function identifiers in tact", ' ', 0},
	{'d', "use diff format for output", ' ', 0},
	{'T', "terse output", ' ', 0},
	{'n', "display headings only", ' ', 0},
	{'p', "use percentage format for output", ' ', 0},
	{'P', "use percentage format, showing all combinations", ' ', 0},
	{'t', "threshold level of percentage to show", 'N', &threshold_string},
	{'e', "compare each file to each file separately", ' ', 0},
	{'s', "do not compare a file to itself", ' ', 0},
	{'S', "compare new files to old files only", ' ', 0},
	{'R', "recurse into subdirectories", ' ', 0},
	{'i', "read arguments (file names) from standard input", ' ', 0},
	{'o', "write output to file F", 'F', &output_name},
	{'M', "show memory usage info", ' ', 0},
	{'-', "lexical scan output only", ' ', 0},
	{0, 0, 0, 0}
};

static void
print_stream(const char *fname) {
	fprintf(Output_File, "File %s:", fname);
	if (!Open_Stream(fname)) {
		fprintf(Output_File, " cannot open\n");
		return;
	}

	if (!is_set_option('T')) {
		fprintf(Output_File,
			" showing token stream:\nnl_cnt, tk_cnt: %ss",
			token_name
		);

		lex_token = End_Of_Line;
		do {
			if (Token_EQ(lex_token, End_Of_Line)) {
				fprintf(Output_File, "\n%u,%u:",
					lex_nl_cnt, lex_tk_cnt
				);
			}
			else {
				fprintf(Output_File, " ");
				fprint_token(Output_File, lex_token);
			}
		} while (Next_Stream_Token_Obtained());

		fprintf(Output_File, "\n");
	}

	Close_Stream();
}

static void
read_and_compare_files(int argc, const char **argv, int round) {
	Read_Input_Files(argc, argv, round);
	Make_Forward_References();
	Compare_Files();
	Free_Forward_References();
}

static void
reverse_new_input_files(int argc, const char *argv[]) {
	int txt_first = 0;
	int txt_last;

	/* find the end of the new files */
	for (txt_last = 0; txt_last < argc; txt_last++) {
		if (strcmp(argv[txt_last], "/") == 0) break;
	}
	txt_last--;

	/* swap the names from the outer sides on */
	while (txt_first < txt_last) {
		const char *tmp = argv[txt_first];
		argv[txt_first] = argv[txt_last];
		argv[txt_last] = tmp;
		txt_first++, txt_last--;
	}
}

int
main(int argc, const char *argv[]) {

	/* Save program name */
	progname = argv[0];
	argv++, argc--;				/* and skip it */

	/* Set the default output and debug streams */
	Output_File = stdout;
	Debug_File = stdout;

	/* Get command line options */
	{	int nop = do_options(progname, optlist, argc, argv);
		argc -= nop, argv += nop;	/* and skip them */
	}

	/* Treat the value options */
	if (min_run_string) {
		Min_Run_Size = strtoul(min_run_string, NULL, 10);
		if (Min_Run_Size == 0)
			fatal("bad or zero run size; form is: -r N");
	}
	if (page_width_string) {
		Page_Width = atoi(page_width_string);
		if (Page_Width == 0)
			fatal("bad or zero page width; form is: -w N");
	}
	if (threshold_string) {
		Threshold_Percentage = atoi(threshold_string);
		if ((Threshold_Percentage > 100) || (Threshold_Percentage <= 0))
			fatal("threshold must be between 1 and 100");
	}
	if (output_name) {
		Output_File = fopen(output_name, "w");
		if (Output_File == 0) {
			char msg[500];

			sprintf(msg, "cannot open output file %s",
				output_name);
			fatal(msg);
			/*NOTREACHED*/
		}
	}

	if (is_set_option('i')) {
		if (argc != 0)
			fatal("-i option conflicts with file arguments");
		get_new_std_input_args(&argc, &argv);
	}
	else if (is_set_option('R')) {
		get_new_recursive_args(&argc, &argv);
	}

	if (is_set_option('P')) {
		Threshold_Percentage = 1;
		set_option('p');
	}

	if (is_set_option('p')) {
		set_option('e');
		set_option('s');
	}

	if (is_set_option('-')) {
		/* it is the lexical scan only */
		while (argv[0]) {
			print_stream(argv[0]);
			argv++;
		}
		return 0;
	}
	/* (argc, argv) now represents new_file* [ / old_file*] */

	/* Here the real work starts */
	Init_Language();

	if (!is_set_option('p')) {
		/* Runs */
		read_and_compare_files(argc, argv, 1);
		Retrieve_Runs();
		Show_Runs();
	} else {
		/* Percentages */
		/* To compute the percentages fairly, the input files are read
		   twice, once in command line order, and once with the new
		   files in reverse order.
		*/
		read_and_compare_files(argc, argv, 1);
		reverse_new_input_files(argc, argv);
		read_and_compare_files(argc, argv, 2);
		Show_Percentages();
	}

	if (is_set_option('M')) {
		/* It is not trivial to plug the leaks, because data structures
		   point to each other, and have to be freed in the proper
		   order. But it is not impossible either. To do, perhaps.
		*/
		ReportMemoryLeaks(stderr);
	}

	return 0;
}
