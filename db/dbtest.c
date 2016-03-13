/*
 * Test program for MySQL API connection
 *
 * gcc -Wall -o dbtest dbtest.c -L/usr/lib64/mysql -lmysqlclient
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <mysql/mysql.h>

#define DBHOST      "moonbase"
#define DBNAME      "sensors"
#define DBUSER      "sensord"

#define SQL_TEXT    "insert into sensor (timestamp, station, sensor, value)" \
                    "values(date_sub(now(), interval ? second), ?, ?, ?)"
#define SQL_NBIND   4

int main(int argc, char*argv[])
{
    MYSQL       *inst;
    MYSQL_STMT  *stmt;
    MYSQL_BIND  params[SQL_NBIND];
    int         i;

    printf("Connecting to database...\n");
    if ((inst = mysql_init(NULL)) == NULL)
    {
        fprintf(stderr, "mysql_init failed: %s\n", mysql_error(inst));
        exit(1);
    }

    if (mysql_real_connect(inst, DBHOST, DBUSER, NULL, DBNAME, 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "mysql_real_connect failed: %s\n", mysql_error(inst));
        exit(1);
    }

    printf("Preparing statement...\n");
    if ((stmt = mysql_stmt_init(inst)) == NULL)
    {
        fprintf(stderr, "mysql_stmt_init failed: %s\n", mysql_error(inst));
        exit(1);
    }
    if (mysql_stmt_prepare(stmt, SQL_TEXT, sizeof(SQL_TEXT)) != 0)
    {
        fprintf(stderr, "mysql_stmt_prepare failed: %s\n", mysql_error(inst));
        exit(1);
    }

    for (i = 0; i < 3; i++)
    {
        int16_t         age         = 50 + i * 10;
        uint8_t         station     = i + 10;
        uint8_t         sensor      = i + 1;
        int16_t         value       = 1000 + i;
        my_ulonglong    nrows;

        memset(params, 0, sizeof(params));

        /* age */
        params[0].buffer_type = MYSQL_TYPE_SHORT;
        params[0].buffer = &age;
        params[0].buffer_length = sizeof(age);
        params[0].is_null = (my_bool *)0;
        params[0].is_unsigned = 0;

        /* station */
        params[1].buffer_type = MYSQL_TYPE_TINY;
        params[1].buffer = &station;
        params[1].buffer_length = sizeof(station);
        params[1].is_null = (my_bool *)0;
        params[1].is_unsigned = 1;

        /* sensor */
        params[2].buffer_type = MYSQL_TYPE_TINY;
        params[2].buffer = &sensor;
        params[2].buffer_length = sizeof(sensor);
        params[2].is_null = (my_bool *)0;
        params[2].is_unsigned = 1;

        /* value */
        params[3].buffer_type = MYSQL_TYPE_SHORT;
        params[3].buffer = &value;
        params[3].buffer_length = sizeof(value);
        params[3].is_null = (my_bool *)0;
        params[3].is_unsigned = 0;

        printf("Binding params for row %d...\n", i + 1);
        if (mysql_stmt_bind_param(stmt, params))
        {
            fprintf(stderr, "mysql_bind_param failed: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }
        printf("Executing statement for row %d...\n", i + 1);
        if (mysql_stmt_execute(stmt))
        {
            fprintf(stderr, "mysql_stmt_execute failed: %s\n", mysql_stmt_error(stmt));
            exit(1);
        }

        nrows = mysql_stmt_affected_rows(stmt);
        printf("(%ld rows inserted)\n", (long)nrows);
    }

    printf("Dropping database connection...\n");
    mysql_close(inst);

    printf("Finished.\n");
    return 0;
}
