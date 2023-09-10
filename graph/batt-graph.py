#!/usr/bin/env python3

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import datetime
import bisect
import sys
import cProfile

class DB_Sensor():
    def __init__(self):
        import mysql.connector as mysql
        self._db = mysql.connect(user='rolfe', db='sensors', charset='utf8')

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self._db.close()

    def raw_samples(self, day, month, year, ndays):
        c = self._db.cursor()
        date = '%04d-%02d-%02d' % (year, month, day)

        sql = '''
            select
                timestamp,
                value / 10.0
            from sensor
            where
                station = 10
                and
                sensor = 6
                and
                timestamp >= str_to_date('%s 00:00:00', '%%Y-%%m-%%d %%H:%%i:%%s')
                and
                timestamp < adddate(str_to_date('%s 00:00:00', '%%Y-%%m-%%d %%H:%%i:%%s'),  %d)
            order
                    by timestamp''' % (date, date, ndays)

        print(sql)

        c.execute(sql)

        (col1, col2) = zip(*c.fetchall())
        c.close()

        return (col1, col2)

def main():
    daynames = [ 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun' ]

    with DB_Sensor() as db:

        day = 19
        month = 3
        year = 2016
        ndays = 1

        secs_per_day = 24 * 60 * 60
        t_sample_secs = 300
        n_samples = int(ndays * secs_per_day / t_sample_secs)

        sdatestr = '%-4d-%02d-%02d 00:00:00' % (year, month, day)

        s = datetime.datetime.strptime(sdatestr, '%Y-%m-%d %H:%M:%S')
        e = s + datetime.timedelta(ndays)

        (col1, col2) = db.raw_samples(day, month, year, ndays)

        # print(len(col1),len(col2))

        raw_times = [ (r - s).total_seconds() / secs_per_day for r in col1 ]
        raw_temps = [ float(r) for r in col2 ]

        times = [ t / n_samples * ndays for t in range(0, n_samples) ]
        temps = []


        xticks = [ t + 0.5 for t in range(0, ndays) ]
        xticks = [ t + 0.5 for t in range(0, ndays) ]
        xlabels = [ daynames[(s + datetime.timedelta(d)).weekday()] for d in range(0, ndays) ]

        print(xticks)
        print(xlabels)

        for s in times:
            i = bisect.bisect(raw_times, s)

            if i == 0 or i == len(raw_times):
                temps.append(None)
            else:
                x1 = raw_times[i-1]
                x2 = raw_times[i]
                if s - x1 <= ndays and x2 - s < ndays:
                    y1 = raw_temps[i-1]
                    y2 = raw_temps[i]
                    y = (s - x1) / ndays * (y2 - y1) + y1;
                    temps.append(y)
                else:
                    temps.append(None)

        # print(times)
        # print(temps)

        fig = plt.figure(1, figsize=(12, 6))

        ax = plt.axes()
        ax.xaxis.set_major_formatter(ticker.NullFormatter())
        ax.xaxis.set_minor_locator(ticker.FixedLocator(xticks))
        ax.xaxis.set_minor_formatter(ticker.FixedFormatter(xlabels))

        plt.title('Weekly temperatures for sensor 10')
        # plt.xticks(xticks, xlabels)
        plt.xlim(0, ndays)
        plt.ylim(2.5, 3.5)
        plt.plot(times, temps, '-')
        plt.show()

    return temps

if __name__ == '__main__':
    values = main()
    print(values)
