#include <stdlib.h>
#include <mysql/mysql.h>
#include "rmfio.h"
#include "db.h"


static MYSQL conn;


/*
   static const char* type_names[] = {
   "MYSQL_TYPE_TINY",
   "MYSQL_TYPE_SHORT",
   "MYSQL_TYPE_LONG",
   "MYSQL_TYPE_INT24",
   "MYSQL_TYPE_LONGLONG",
   "MYSQL_TYPE_DECIMAL",
   "MYSQL_TYPE_NEWDECIMAL",
   "MYSQL_TYPE_FLOAT",
   "MYSQL_TYPE_DOUBLE",
   "MYSQL_TYPE_BIT",
   "MYSQL_TYPE_TIMESTAMP",
   "MYSQL_TYPE_DATE",
   "MYSQL_TYPE_TIME",
   "MYSQL_TYPE_DATETIME",
   "MYSQL_TYPE_YEAR",
   "MYSQL_TYPE_STRING",
   "MYSQL_TYPE_VAR_STRING",
   "MYSQL_TYPE_BLOB",
   "MYSQL_TYPE_SET",
   "MYSQL_TYPE_ENUM",
   "MYSQL_TYPE_GEOMETRY",
   "MYSQL_TYPE_NULL"
   };

   static int type_values[] = {
   MYSQL_TYPE_TINY,
   MYSQL_TYPE_SHORT,
   MYSQL_TYPE_LONG,
   MYSQL_TYPE_INT24,
   MYSQL_TYPE_LONGLONG,
   MYSQL_TYPE_DECIMAL,
   MYSQL_TYPE_NEWDECIMAL,
   MYSQL_TYPE_FLOAT,
   MYSQL_TYPE_DOUBLE,
   MYSQL_TYPE_BIT,
   MYSQL_TYPE_TIMESTAMP,
   MYSQL_TYPE_DATE,
   MYSQL_TYPE_TIME,
   MYSQL_TYPE_DATETIME,
   MYSQL_TYPE_YEAR,
   MYSQL_TYPE_STRING,
   MYSQL_TYPE_VAR_STRING,
   MYSQL_TYPE_BLOB,
   MYSQL_TYPE_SET,
   MYSQL_TYPE_ENUM,
   MYSQL_TYPE_GEOMETRY,
   MYSQL_TYPE_NULL
   };
*/

static char json_construct_buffer[1024 * 1024 * 512];
static char* jsonbufp;



static void push_reset(void)
{
	jsonbufp = json_construct_buffer;
	jsonbufp += sprintf(jsonbufp, "{ \"data\": [ ");
}

static void push_end(void)
{
	jsonbufp += sprintf(jsonbufp, "]}");
	*jsonbufp = '\0';
}

static void push_row(
		MYSQL_FIELD* const fields,
		const int num_fields,
		MYSQL_ROW row
)
{
	static int is_first = 1;

	if (!is_first) {
		jsonbufp += sprintf(jsonbufp, ",{");
	} else {
		jsonbufp += sprintf(jsonbufp, "{");
		is_first = 0;
	}

	for (int i = 0; i < num_fields; ++i) {
		if (i > 0)
			jsonbufp += sprintf(jsonbufp, ",");

		MYSQL_FIELD* const field = &fields[i];
		if (!IS_NUM(field->type)) {
			jsonbufp += sprintf(jsonbufp, "\"%s\":\"%s\"", field->name, row[i]);
		} else {
			jsonbufp += sprintf(jsonbufp, "\"%s\": %s", field->name, row[i]);
		}

	}

	jsonbufp += sprintf(jsonbufp, "}");
}





void db_init(
		const char* const db_host,
		const unsigned int db_port,
		const char* const db_user,
		const char* const db_name,
		const char* const db_pass
)
{
	if (mysql_init(&conn) == NULL) {
		FIO_LOG_FATAL("Failed on mysql_init()\n");
		exit(1);
	}

	if (mysql_real_connect(&conn, db_host, db_user,
	                       db_pass, db_name, db_port, NULL, 0) == NULL) {
		FIO_LOG_FATAL("Failed to connect to database: Error: %s\n", mysql_error(&conn));
		exit(1);
	}
}


void db_term(void)
{
	mysql_close(&conn);
}

FIOBJ db_select(const char* const query)
{
	if (mysql_query(&conn, query)) {
		FIO_LOG_ERROR("mysql query failed: %s", mysql_error(&conn));
		return FIOBJ_INVALID;
	}

	MYSQL_RES* const result = mysql_store_result(&conn);

	if (result == NULL) {
		FIO_LOG_ERROR("mysql store result error: %s", mysql_error(&conn)); 
		return FIOBJ_INVALID;	
	}

	const int num_fields = mysql_num_fields(result);

	MYSQL_ROW row;
	MYSQL_FIELD* const fields = mysql_fetch_fields(result);

	push_reset();

	while ((row = mysql_fetch_row(result)) != NULL) {
		push_row(fields, num_fields, row);
	}

	push_end();

	mysql_free_result(result);

	// FIO_LOG_INFO("DB SELECT QUERY %s RESULTS: ", query);
	// FIO_LOG_INFO("%s", json_construct_buffer);

	FIOBJ obj = FIOBJ_INVALID;

	const size_t consumed = fiobj_json2obj(
			&obj,
			json_construct_buffer,
			strlen(json_construct_buffer)
	);

	if (!consumed || !obj) {
		FIO_LOG_FATAL("Could't parse data");
		return FIOBJ_INVALID;
	}

	return obj;
}




