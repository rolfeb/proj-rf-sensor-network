#!/usr/bin/env python3

import mysql.connector as mysql
import numpy as np
import datetime
import sys

db = mysql.connect(user='rolfe', db='sensors', charset='utf8')

c = db.cursor()
c.execute('''
select
    timestamp,
    value / 10.0
from sensor
where
    station = 10
    and
    sensor = 1
    and
    timestamp >= str_to_date('2016-03-13 00:00:00', '%Y-%m-%d %H:%i:%s')
    and
    timestamp < str_to_date('2016-03-14 00:00:00', '%Y-%m-%d %H:%i:%s')
order
    by timestamp''')

rows = list(c.fetchall())
nrows = len(rows)

s = datetime.datetime.strptime('2016-03-13 00:00:00', '%Y-%m-%d %H:%M:%S')
e = datetime.datetime.strptime('2016-03-14 00:00:00', '%Y-%m-%d %H:%M:%S')
t = [ t / (24*60*60) for t in range(0, (24*60*60), 300) ]

times = [ (r[0] - s).total_seconds() / (24 * 60 * 60) for r in rows ]
temps = [ float(r[1]) for r in rows ]

interp_rows = np.interp(t, times, temps, left=None, right=None)

print(type(interp_rows))
print(type(interp_rows[0]))

data_type = [('value', np.float)]

data = np.fromiter(interp_rows, count=-1, dtype=data_type)
print(data)

c.close()

db.close()
