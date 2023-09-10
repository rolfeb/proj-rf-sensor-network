#!/usr/bin/env python3

import numpy as np
import pandas as pd
import matplotlib as mpl
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
                sensor = 1
                and
                timestamp >= str_to_date('%s 00:00:00', '%%Y-%%m-%%d %%H:%%i:%%s')
                and
                timestamp < adddate(str_to_date('%s 00:00:00', '%%Y-%%m-%%d %%H:%%i:%%s'),  %d)
            order
                    by timestamp''' % (date, date, ndays)

        print(sql)

        c.execute(sql)

        #
        # Convert the returned values to the required data types
        #
        cols = [ (pd.Timestamp(c[0]), float(c[1])) for c in c.fetchall() ]

        #
        # Construct a pandas.Series() to hold the data
        #
        (col1, col2) = zip(*cols)
        ts = pd.Series(col2, index=col1)

        c.close()

        return ts

def main():
    daynames = [ 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun' ]

    with DB_Sensor() as db:

        (year, month, day) = (2016, 3, 12)
        ndays = 2

        #
        # Calculate the start/end of the range as Timestamp values
        #
        sdatestr = '%-4d-%02d-%02d 00:00:00' % (year, month, day)
        s = pd.Timestamp(datetime.datetime.strptime(sdatestr, '%Y-%m-%d %H:%M:%S'))
        e = s + datetime.timedelta(ndays)

        #
        # Get the raw data from the database
        #
        ts = db.raw_samples(day, month, year, ndays)

        ts = ts.resample('5Min')

        #
        # Plot the data
        #
        fig = plt.figure(1, figsize=(12, 6))
        ax = plt.axes()

        ax.plot_date(ts.keys().to_pydatetime(), ts, '-')

        xfmt = mpl.dates.DateFormatter('%H:%M')
        ax.xaxis.set_major_formatter(xfmt)

        plt.title('Weekly temperatures for sensor 10')
        plt.xlim(s, e)
        plt.ylim(10, 30)
        plt.plot(ts, '-')
        fig.tight_layout()

        plt.show()


    return None

if __name__ == '__main__':
    values = main()
    print(values)
