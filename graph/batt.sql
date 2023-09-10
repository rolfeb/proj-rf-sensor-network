select
    station, timestamp, value
from
    sensor
where
    sensor = 6
order by
    timestamp desc
limit 1
;
