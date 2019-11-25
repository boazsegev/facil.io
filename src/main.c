#include "main.h"


int main(int argc, char const *argv[]) 
{
	/* accept command line arguments and setup default values, see "cli.c" */
	initialize_cli(argc, argv);

	initialize_services();

	/* start facil */
	fio_start(.threads = fio_cli_get_i("-t"), .workers = fio_cli_get_i("-w"));

	/* cleanup CLI, see "cli.c" */
	free_cli();
	return 0;
}

