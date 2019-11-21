#ifndef RMFIO_DB_H_
#define RMFIO_DB_H_
#include "fio.h"
#include "fiobj.h"

extern void db_init(
	const char* db_host,
	unsigned int db_port,
	const char* db_user,
	const char* db_name,
	const char* db_pass
);

extern void db_term(void);


extern FIOBJ db_select(const char* query);



#endif
