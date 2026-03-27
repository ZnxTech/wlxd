#include <stdio.h>
#include <string.h>

const char *help_str =
	"Usage: wlxd [OPTION...]\n"
	"\n"
	"	-h, --help		give this help list\n";

void arg_help(void)
{
	puts(help_str);
}

void handle_invalid_arg(char *arg)
{
	printf("invalid argument: \"%s\"\n", arg);
}

void handle_arg(int	   argc,
				char **argv)
{
	char *arg = argv[0];
	argc--;
	argv++;

	if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
		arg_help();
	else
		handle_invalid_arg(arg);
}

void handle_args(int	argc,
				 char **argv)
{
	// no arguments
	if (argc <= 1)
		return;

	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		handle_arg(argc - i, argv + i);
	}
}

int main(int	argc,
		 char **argv)
{
	handle_args(argc, argv);

	return 0;
}
