drop table if exists sensor;

create table sensor
(
    timestamp   datetime            not null,

    -- Definitions for wireless sensor messages
    --
    --  uint8_t     id          Unique station ID
    --  uint8_t     nvalues     Number of sensor readings
    --
    --  nvalues * [
    --  uint8_t     type        Sensor type (WL_SENSOR_TYPE_*)
    --  int16_t     value       Sensor value (little endian)
    --  ]

    station     tinyint unsigned    not null,
    sensor      tinyint unsigned    not null,
    value       smallint            not null,

    index sensor_1 (timestamp, station, sensor),
    index sensor_2 (timestamp, sensor, station)
)
engine=MyISAM default charset=utf8 collate=utf8_bin;
