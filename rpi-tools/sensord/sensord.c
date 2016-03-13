/*
 * Daemon to monitor sensor messages and insert into a MySQL database table.
 *
 * Sensord periodically polls the status of the sensor receiver via an I2C interface.
 * Any changes to sensor status result in updates to a remote MySQL database. Sensord
 * can be halted by sending it a TERM signal.
 *
 * gcc -Wall -I../../include -o sensord sensord.c -lmysqlclient
 */

#define _XOPEN_SOURCE   /* for sigaction, daemon */

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <mysql/mysql.h>

#include "wireless.h"

/**
 * I2C device name
 */
static const char       *I2C_DEVICE         = "/dev/i2c-0";

/**
 * RPi receiver slave address
 */
static const int        I2C_SLAVE_ADDRESS   = 0x41;

/**
 * The host where the MySQL database resides.
 */
static const char       *DB_HOST            = "moonbase";

/**
 * The name of the database
 */
static const char       *DB_NAME            = "sensors";

/**
 * The username to connect to the database.  No password should be required
 * to connect and insert rows.
 */
static const char       *DB_USER            = "sensord";

/**
 * The text of the SQL insert statement
 */
static const char       *SQL_TEXT           = "insert into sensor (timestamp, station, sensor, value)"
                                                "values(date_sub(now(), interval ? second), ?, ?, ?)";

/**
 * The number of bind parameters in the above statement.
 */
static const int        SQL_NBIND           = 4;    /* must match the statement above */

/**
 * The time in seconds after which we regard a station as dead if we haven't
 * received a message from it.
 */
static const int16_t    STATION_DEAD_THRESHOLD  = 600;

/**
 * If the station battery drops to this level, log a warning
 */
static const int16_t    STATION_LOWBATT_THRESHOLD   = 26;

/**
 * If the station battery rises to this level, log a notice
 */
static const int16_t    STATION_OKBATT_THRESHOLD    = 28;

/**
 * Station state flag - station is off the air
 */
static const uint8_t    STATION_FLAG_DEAD       = 0x1;

/**
 * Station state flag - station has a low battery
 */
static const uint8_t    STATION_FLAG_LOWBATT    = 0x2;

typedef uint8_t             station_state_t;
typedef struct reading_t    reading_t;

/**
 * A structure to keep track of the most recent sensor readings from
 * each station.
 */
struct reading_t
{
    /** station ID */
    uint8_t             station;

    /** sensor type */
    uint8_t             sensor;

    /** seqno when we received value */
    int16_t             seqno;

    /** next entry in list */
    reading_t           *next;
};

/**
 * A flag set by signal handlers to indicate that we should terminate.
 */
static volatile int Shutdown            = 0;

/**
 * Signal handler for shutting down the daemon.
 *
 * @param[in]   signum  The signal that we are handling.
 */
static void
set_shutdown_flag(int signum)
{
    Shutdown = 1;
}

/**
 * Connect to the database and create a prepared insert statement.
 *
 * @param[out]  inst_p  The address of a MySQL database instance pointer.
 * @param[out]  stmt_p  The address of a MySQL statement handle pointer.
 *
 * @return      zero for success, non-zero otherwise.
 */
static int
db_start(MYSQL **inst_p, MYSQL_STMT **stmt_p)
{
    MYSQL       *inst;
    MYSQL_STMT  *stmt;

    if ((inst = mysql_init(NULL)) == NULL)
        return 1;

    if (mysql_real_connect(inst, DB_HOST, DB_USER, NULL, DB_NAME, 0, NULL, 0) == NULL)
    {
        mysql_close(inst);
        return 2;
    }

    if ((stmt = mysql_stmt_init(inst)) == NULL)
    {
        mysql_close(inst);
        return 3;
    }
    
    if (mysql_stmt_prepare(stmt, SQL_TEXT, strlen(SQL_TEXT)) != 0)
    {
        mysql_stmt_close(stmt);
        mysql_close(inst);
        return 4;
    }

    *inst_p = inst;
    *stmt_p = stmt;

    return 0;
}

/**
 * Insert a new row into the database.
 *
 * @param[in]   stmt            The prepared database insert statement.
 * @param[in]   station_id      The station ID.
 * @param[in]   sensor_type     The sensor type.
 * @param[in]   sensor_value    The sensor value.
 * @param[in]   age             The age of the sensor reading, in seconds.
 */
static bool
db_insert
(
    MYSQL_STMT  *stmt,
    uint8_t     station_id,
    uint8_t     sensor_type,
    int16_t     sensor_value,
    int16_t     age
)
{
    MYSQL_BIND  params[SQL_NBIND];

    memset(params, 0, sizeof(params));

    /* age */
    params[0].buffer_type = MYSQL_TYPE_SHORT;
    params[0].buffer = &age;
    params[0].buffer_length = sizeof(age);
    params[0].is_null = (my_bool *)0;
    params[0].is_unsigned = 0;

    /* station */
    params[1].buffer_type = MYSQL_TYPE_TINY;
    params[1].buffer = &station_id;
    params[1].buffer_length = sizeof(station_id);
    params[1].is_null = (my_bool *)0;
    params[1].is_unsigned = 1;

    /* sensor */
    params[2].buffer_type = MYSQL_TYPE_TINY;
    params[2].buffer = &sensor_type;
    params[2].buffer_length = sizeof(sensor_type);
    params[2].is_null = (my_bool *)0;
    params[2].is_unsigned = 1;

    /* value */
    params[3].buffer_type = MYSQL_TYPE_SHORT;
    params[3].buffer = &sensor_value;
    params[3].buffer_length = sizeof(sensor_value);
    params[3].is_null = (my_bool *)0;
    params[3].is_unsigned = 0;

    if (mysql_stmt_bind_param(stmt, params))
        return false;

    if (mysql_stmt_execute(stmt))
        return false;

    return true;
}

/**
 * Clean up our connection to the MySQL database.
 *
 * @param[in]   inst    A MySQL database instance pointer.
 * @param[in]   stmt    A MySQL statement handle pointer.
 */
static void
db_end(MYSQL *inst, MYSQL_STMT *stmt)
{
    mysql_stmt_close(stmt);
    mysql_close(inst);
}

/**
 * Check to see if this is a new reading from the station sensor.
 *
 * @param[in]       station         The station ID.
 * @param[in]       sensor          The sensor type.
 * @param[in]       seqno           The seqno for the latest sensor value.
 * @param[in,out]   sensor_state    List of current sensor states.
 *
 * @return true if this is a new sensor value, false otherwise.
 */
static bool
sensor_changed
(
    uint8_t     station,
    uint8_t     sensor,
    int16_t     seqno,
    reading_t   **sensor_state
)
{
    reading_t   *r;

    /*
     * Look for a matching sensor state record. If found, tell the caller
     * whether the seqno has changed (a new reading was received).
     */
    for (r = *sensor_state; r != NULL; r = r->next)
    {
        if (r->station == station && r->sensor == sensor)
        {
            if (r->seqno != seqno)
            {
                r->seqno = seqno;
                return true;
            }
            else
                return false;
        }
    }

    /*
     * This must be a new station/sensor reading; add it to our state.
     */
    if ((r = malloc(sizeof(reading_t))) == NULL)
        return false;   /* dodgy, I know */

    r->station = station;
    r->sensor = sensor;
    r->seqno = seqno;
    r->next = *sensor_state;
    *sensor_state = r;

    return true;
}

/**
 * Process a message from the RPi receiver
 *
 * @param[in]       message         The message data.
 * @param[in]       length          The length of the message data.
 * @param[in,out]   station_state   List of current station states.
 * @param[in,out]   sensor_state    List of current sensor states.
 * @param[in]       stmt            The prepared database insert statement.
 *
 * @return      zero for success, non-zero otherwise.
 */
static bool
process_message
(
    const char      *message,
    int             length,
    station_state_t *station_state,
    reading_t       **sensor_state,
    MYSQL_STMT      *stmt
)
{
    uint8_t     n_stations;
    uint8_t     station_id;
    uint8_t     n_sensors;
    uint8_t     sensor_type;
    int16_t     sensor_value;
    int16_t     age;
    int16_t     seqno;
    int16_t     battery;
    uint8_t     i;
    uint8_t     j;
    const char  *p;

    /*
     * Message format (shorts are LSB first):
     *
     * len
     *  1   0x01
     *  1   message length
     *  1   number of stations
     *
     *  1   station 1 id
     *  1   number of sensors (n)
     *  1   sensor type 1
     *  2   sensor value 1
     *          [...]
     *  1   sensor type n
     *  2   sensor value n
     *  2   age
     *
     *  2   station 2 id
     *          [... etc]
     */
    n_stations = *(uint8_t *)&message[2];

    p = message + 3;
    for (i = 0; i < n_stations; ++i)
    {
        station_id  = *(uint8_t *)&p[0];
        n_sensors   = *(uint8_t *)&p[1];
        p += 2;

        /*
         * Only consider valid station IDs
         */
        if (station_id != 0 && station_id != 255)
        {
            /*
             * The age of the datagram from the sender is after all the
             * sensor readings.
             */
            age = *(uint16_t *)(p + n_sensors * 3);

            /*
             * Log messages if a station dies or revives.
             */
            if (age > STATION_DEAD_THRESHOLD)
            {
                if ((station_state[station_id] & STATION_FLAG_DEAD) == 0)
                {
                    syslog(LOG_ERR, "error: no message from station %d for %d seconds", station_id, age);
                    station_state[station_id] |= STATION_FLAG_DEAD;
                }
            }
            else
            {
                if ((station_state[station_id] & STATION_FLAG_DEAD) != 0)
                {
                    syslog(LOG_NOTICE, "message received from previously dead station %d", station_id);
                    station_state[station_id] &= ~STATION_FLAG_DEAD;
                }
            }

            /*
             * Extract some standard sensor information.
             */
            seqno = -1;
            battery = -1;
            for (j = 0; j < n_sensors; j++)
            {
                sensor_type     = *(uint8_t *)(p + j*3 + 0);
                sensor_value    = *(int16_t *)(p + j*3 + 1);

                if (sensor_type == WL_SENSOR_TYPE_COUNTER)
                    seqno = sensor_value;
                else if (sensor_type == WL_SENSOR_TYPE_BATTERY)
                    battery = sensor_value;
            }

            /*
             * Process the various sensor values
             */
            for (j = 0; j < n_sensors; j++)
            {
                sensor_type     = *(uint8_t *)(p + j*3 + 0);
                sensor_value    = *(int16_t *)(p + j*3 + 1);

                if (sensor_type == WL_SENSOR_TYPE_COUNTER)
                    continue;

                /*
                 * If this sensor is a newer reading from the last time we
                 * checked, then update the database with the new value.
                 */
                if (sensor_changed(station_id, sensor_type, seqno, sensor_state))
                {
                    if (!db_insert(stmt, station_id, sensor_type, sensor_value, age))
                        return false;
                }
            }

            /*
             * Log messages if a station battery enters/leaves low voltage state
             */
            if (battery != -1)
            {
                if (battery <= STATION_LOWBATT_THRESHOLD)
                {
                    if ((station_state[station_id] & STATION_FLAG_LOWBATT) == 0)
                    {
                        syslog(LOG_ERR, "error: low battery warning from station %d (%.1fV) ",
                            station_id, battery / 10.0);
                        station_state[station_id] |= STATION_FLAG_LOWBATT;
                    }
                }
                else
                if (battery >= STATION_OKBATT_THRESHOLD)
                {
                    if ((station_state[station_id] & STATION_FLAG_LOWBATT) != 0)
                    {
                        syslog(LOG_NOTICE, "normal battery level restored for station %d (%.1fV)",
                            station_id, battery / 10.0);
                        station_state[station_id] &= ~STATION_FLAG_LOWBATT;
                    }
                }
            }
        }

        p += n_sensors * 3; /* skip sensor type/values */
        p += 2;             /* skip age */
    }

    return true;
}

int
main(int argc, char*argv[])
{
    int                 i2c_device;
    MYSQL               *inst;
    MYSQL_STMT          *stmt;
    station_state_t     station_state[256];
    reading_t           *sensor_state   = NULL;
    struct sigaction    sigact;
    int                 i;

    openlog("sensord", 0, LOG_LOCAL1);

    if ((i2c_device = open(I2C_DEVICE, O_RDWR)) < 0)
    {
        fprintf(stderr, "Failed to open %s: %s\n", I2C_DEVICE, strerror(errno));
        return 1;
    }
    if (ioctl(i2c_device, I2C_SLAVE, (long)I2C_SLAVE_ADDRESS) < 0)
    {
        fprintf(stderr, "ioctl(I2C_SLAVE) failed: %s\n", strerror(errno));
        close(i2c_device);
        return 1;
    }

    if (db_start(&inst, &stmt) != 0)
    {
        fprintf(stderr, "Database initialisation failed\n");
        return 1;
    }

    /*
     * Set up signal handling:
     *  ignore HUP
     *  terminate on INT, TERM
     */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    sigact.sa_handler = SIG_IGN;
    sigaction(SIGHUP, &sigact, NULL);

    sigact.sa_handler = set_shutdown_flag;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);

    memset(station_state, 0, sizeof(station_state));

    syslog(LOG_INFO, "started; entering event loop");

    /*
     * Detach ourselves from the controlling tty
     */
    daemon(0, 0);

    /*
     * Main event loop
     */
    while (!Shutdown)
    {
        char    i2c_message[256];
        int     n;

        /*
         * Read current state from the sensor receiver
         */
        if ((n = read(i2c_device, i2c_message, sizeof(i2c_message))) < 0)
        {
            fprintf(stderr, "message read failed: %s\n", strerror(errno));
            return 1;
        }

        if (!process_message(i2c_message, n, station_state, &sensor_state, stmt))
        {
            fprintf(stderr, "message process failed\n");
            return 1;
        }

        /*
         * Sensors send messages every 64 seconds, so this will ensure we don't miss 
         * any updates.
         */
        for (i = 0; !Shutdown && i < 45; ++i)
            sleep(1);
    }

    db_end(inst, stmt);

    close(i2c_device);

    syslog(LOG_INFO, "terminating");
    closelog();

    return 0;
}
