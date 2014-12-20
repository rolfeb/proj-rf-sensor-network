/*
 * Wireless Receiver for Raspberry Pi
 *
 * Copyright: Rolfe Bozier, rolfe@pobox.com, 2012
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define I2C_DEVICE          "/dev/i2c-0"
#define I2C_SLAVE_ADDR      0x41

#define N_SENSOR_TYPES      5

struct sensor_t
{
    int     valid;
    int     value;
};

static void
write_as_csv(char *message, int bytes_read)
{
    int             i;
    int             j;
    int             n_stations;
    struct sensor_t sensors[N_SENSOR_TYPES];
    char            *ptr;
    
    ptr = &message[2];
    n_stations = *ptr++;

    for (i = 0; i < n_stations; i++)
    {
        int     station_id;
        int     n_sensors;
        int     age_offset;
        int     age;

        station_id = *ptr++;
        n_sensors = *ptr++;
        age_offset = n_sensors * 3;
        age = (ptr[age_offset + 1] << 8) + ptr[age_offset];

        for (j = 0; j < N_SENSOR_TYPES; j++)
            sensors[j].valid = 0;

        for (j = 0; j < n_sensors; j++)
        {
            int     type;
            int     value;

            type = ptr[0];
            value = (ptr[2] << 8) + ptr[1];
            ptr += 3;

            sensors[type - 1].valid = 1;
            sensors[type - 1].value = value;
        }
        ptr += 2;   // age

        printf("%d,%d,", station_id, age);

        for (j = 0; j < N_SENSOR_TYPES; j++)
        {
            if (sensors[j].valid)
                printf("%d", sensors[j].value);
            printf(",");
        }

        printf("\n");
    }
}

static void
write_as_text(char *message, int bytes_read)
{
    int     i;
    int     j;
    int     n_stations;
    char    *ptr;

    printf("Message bytes=%d type=%d length=%d\n",
        bytes_read, message[0], message[1]);

    ptr = &message[2];
    n_stations = *ptr++;

    for (i = 0; i < n_stations; i++)
    {
        int     station_id;
        int     n_sensors;
        int     sensor_type;
        int     sensor_value;
        int     age;

        station_id = *ptr++;
        n_sensors = *ptr++;

        printf("Station [%d]\n", station_id);
        for (j = 0; j < n_sensors; j++)
        {
            sensor_type = ptr[0];
            sensor_value = (ptr[2] << 8) + ptr[1];
            ptr += 3;

            printf("    sensor type %d = %d\n", sensor_type, sensor_value);
        }

        age = (ptr[1] << 8) + ptr[0];
        ptr += 2;

        printf("  [last message: %d secs ago]\n", age);
        printf("\n");
    }
        
    for (i = 0; i < message[1]; i++)
    {
        printf("%02x ", message[i+2]);
        if (i % 16 == 15)
            printf("\n");
    }
    printf("\n");
}

int
main(int argc, char **argv)
{
    int     opt;
    int     csv_mode    = 0;
    int     dev;
    char    message[1024];
    int     n;

    while ((opt = getopt(argc, argv, "hc")) != -1)
    {
        switch (opt)
        {
        case 'c':
            csv_mode = 1;
            break;
        default:
            printf("Usage: %s [-c]\n", argv[0]);
            printf("\t-c\tWrite output as CSV format\n");
            return 1;
        }
    }

    if ((dev = open(I2C_DEVICE, O_RDWR)) < 0)
    {
        fprintf(stderr, "%s: failed to open %s: %s\n",
            argv[0], I2C_DEVICE, strerror(errno));
        return 1;
    }

    if (ioctl(dev, I2C_SLAVE, (long)I2C_SLAVE_ADDR) < 0)
    {
        fprintf(stderr, "%s: ioctl(I2C_SLAVE) failed: %s\n",
            argv[0], strerror(errno));
        close(dev);
        return 1;
    }

    if ((n = read(dev, message, 1024)) < 0)
    {
        fprintf(stderr, "%s: message read failed: %s\n",
            argv[0], strerror(errno));
        close(dev);
        return 1;
    }

    if (csv_mode)
        write_as_csv(message, n);
    else
        write_as_text(message, n);

    close(dev);

    return 0;
}
