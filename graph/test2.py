#!/usr/bin/env python3

import mysql.connector as mysql
import numpy as np
import matplotlib.pyplot as plt
import datetime
import bisect
import sys
import cProfile

def get_samples():
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
        timestamp >= str_to_date('2016-03-14 00:00:00', '%Y-%m-%d %H:%i:%s')
        and
        timestamp < str_to_date('2016-03-15 00:00:00', '%Y-%m-%d %H:%i:%s')
    order
        by timestamp''')

    (col1, col2) = zip(*c.fetchall())

    s = datetime.datetime.strptime('2016-03-14 00:00:00', '%Y-%m-%d %H:%M:%S')
    e = datetime.datetime.strptime('2016-03-15 00:00:00', '%Y-%m-%d %H:%M:%S')

    raw_times = [ (r - s).total_seconds() for r in col1 ]
    raw_temps = [ float(r) for r in col2 ]

    times = range(0, (24*60*60), 300)
    temps = []

    for s in times:
        i = bisect.bisect(raw_times, s)

        if i == 0 or i == len(raw_times):
            temps.append(None)
        else:
            x1 = raw_times[i-1]
            x2 = raw_times[i]
            if s - x1 <= 300 and x2 - s < 300:
                y1 = raw_temps[i-1]
                y2 = raw_temps[i]
                y = (s - x1) / 300 * (y2 - y1) + y1;
                temps.append(y)
            else:
                temps.append(None)

    print(len(times), len(temps))

    plt.ylim(10, 40)
    plt.plot(times, temps, '-')
    plt.show()


    c.close()
    db.close()

    return temps

if __name__ == '__main__':
    values = get_samples()
    print(values)
