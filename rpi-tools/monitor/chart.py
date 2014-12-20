#!/usr/bin/env python
# -*- coding: utf_8 -*-
#
# Regenerate rrd graphs for sensors
#
# This should be run from cron at the following intervals:
#   every minute    $0 minute        (update day graphs)
#   every hour      $0 hour          (update week, month graphs)
#   every day       $0 day           (update year graphs)
#

import sys
import cgi
import cgitb
import argparse
import rrdtool
import datetime
###import pytz
###import ephem
import imp


#
# Default parameters for rrdgraph
#
DEF_WIDTH       = 700
DEF_HEIGHT      = 300
DEF_FONT        = 'TITLE:10:Verdana'
DEF_COLOR_BACK  = 'BACK#e0f0ff'

std_format_args = [
    '--imgformat', 'PNG',
    '--width', str(DEF_WIDTH),
    '--height', str(DEF_HEIGHT),
    '--full-size-mode',
    '--font',  DEF_FONT,
    '--color', DEF_COLOR_BACK,
    '--border', '0',
    '--dynamic-labels',
    '--rigid',
]

#
# List of colours to assign to sensors
#
chart_colours = [ 'ff0000', '008000', '000080', '804000', '004040', '800040' ]

#
# Load sensor configuration
#
(file, path, desc) = imp.find_module("sensor-cfg",
    [ ".", "/etc", ])
cfg = imp.load_module("sensors", file, path, desc)

def match_sensors(select):
    """
    Match sensors against the given dictionary of match constraints.
    @param match    Dict containing parameter to match against the sensors
    @return         List of [id, attr] from sensors.cfg
    """

    def sensor_cmp(a, b):
        if a[1]['sort'] < b[1]['sort']:
            return -1
        elif a[1]['sort'] > b[1]['sort']:
            return 1
        else:
            return 0

    list = []

    for (id, attr) in cfg.sensors.items():
        match = True
        for (k, v) in select.items():
            if not attr.has_key(k) or attr[k] != v:
                match = False
                break
        if match:
            list.append([id, attr])

    list.sort(cmp=sensor_cmp)

    return list if len(list) > 0 else None

def calc_night_shading(rrdfile, defvar, cdefs, plots):
    """
    Calculate the shading actions to show nighttime on the graph
    @param rrdfile  The RRD file that we are plotting
    @param defvar   The name of an RRD DEF variable
    @param cdefs    The CDEF action array
    @param plots    The plot action array
    """

    # get time
    dt = datetime.datetime.utcnow()
    print repr(dt)

    sydney = ephem.city('Sydney')
    sydney.date = ephem.Date(dt)
    print sydney.date, '=', ephem.localtime(sydney.date)

    next_r_dt = sydney.next_rising(ephem.Sun()).datetime()
    next_s_dt = sydney.next_setting(ephem.Sun()).datetime()

    next_r_dt = pytz.UTC.localize(next_r_dt)

    print 'rise', next_r_dt.strftime("%s"), repr(next_r_dt)
    print 'set', next_s_dt.strftime("%s"), repr(next_s_dt)

    next_r_jd = int(next_r_dt.strftime('%s'))
    next_s_jd = int(next_s_dt.strftime('%s'))

    if next_r_jd < next_s_jd:
        s1_jd = next_s_jd - 2 * (24 * 60 * 60)
        r1_jd = next_r_jd - 1 * (24 * 60 * 60)

        s2_jd = next_s_jd - 1 * (24 * 60 * 60)
        r2_jd = next_r_jd

        cdefs.append('CDEF:night1=%s,TIME,EXC,POP,%d,%d,LIMIT,UN,0,100,IF' % \
            (defvar, s1_jd, r1_jd))
        plots.append('AREA:night1#e0e0e080')

        cdefs.append('CDEF:night2=%s,TIME,EXC,POP,%d,%d,LIMIT,UN,0,100,IF' % \
            (defvar, s2_jd, r2_jd))
        plots.append('AREA:night2#e0e0e080')
    else:
        s1_jd = next_s_jd - 1 * (24 * 60 * 60)
        r1_jd = next_r_jd - 1 * (24 * 60 * 60)

        print s1_jd, r1_jd

        cdefs.append('CDEF:night1=%s,TIME,EXC,POP,%d,%d,LIMIT,UN,0,100,IF' % \
            (defvar, s1_jd, r1_jd))
        plots.append('AREA:night1#e0e0e080')

def plot_chart(area, metric, period, rrddir, imgdir):

    if period == '1d':
        (img_suffix, graph_start, show_night) = ('1d', '-1d', True)
    elif period == '1w':
        (img_suffix, graph_start, show_night) = ('1w', '-1w', True)
    elif period == '1m':
        (img_suffix, graph_start, show_night) = ('1m', '-1m', False)
    elif period == '6m':
        (img_suffix, graph_start, show_night) = ('6m', '-6m', False)
    elif period == '1y':
        (img_suffix, graph_start, show_night) = ('1y', '-1y', False)
    else:
        raise LookupError("invalid period: ", period)

    c = 0
    defs = []
    cdefs = []
    plots = []

    if metric == 'temp':
        #
        # Regenerate temperature chart
        #
        if imgdir is not None:
            img = "%s/%s-temp-%s.png" % (imgdir, area, img_suffix)
        else:
            img = ""

        title = "%s temperature sensors" % area.title()

        matching_sensors = match_sensors({'area': area, 'temp': True})
        if matching_sensors is None:
            return

        for (id, attr) in matching_sensors:
            location = attr['location']
            rrdfile = "%s/station%s.rrd" % (rrddir, id)
            defvar = "t%s" % id

            defs.append("DEF:%s=%s:temp:AVERAGE" % (defvar, rrdfile))
            cdefs.append("CDEF:%ssmooth=%s,1800,TREND" % (defvar, defvar))
            plots.append("LINE:%ssmooth#%s:%s\l" % \
                (defvar, chart_colours[c], location))

            c = c + 1

        #if show_night:
        #    calc_night_shading(rrdfile, defvar, cdefs, plots)

        rrdtool.graph(img,
            std_format_args,
            '--vertical-label', 'C',
            '--upper-limit', '40',
            '--lower-limit', '0',
            '--title', title,
            '--start', graph_start,
            defs, cdefs, plots
        )

    elif metric == 'pres':
        #
        # Regenerate pressure chart
        #
        if imgdir is not None:
            img = "%s/%s-pres-%s.png" % (imgdir, area, img_suffix)
        else:
            img = ""

        title = "%s pressure sensors" % area.title()

        matching_sensors = match_sensors({'area': area, 'pres': True})
        if matching_sensors is None:
            return

        for (id, attr) in matching_sensors:
            location = attr['location']
            rrdfile = "%s/station%s.rrd" % (rrddir, id)
            defvar = "p%s" % id

            defs.append("DEF:%s=%s:pres:AVERAGE" % (defvar, rrdfile))
            cdefs.append("CDEF:%ssmooth=%s,1800,TREND" % (defvar, defvar))
            plots.append("LINE:%ssmooth#%s:%s\l" % \
                (defvar, chart_colours[c], location))

            c = c + 1

        #if show_night:
        #    calc_night_shading(rrdfile, defvar, cdefs, plots)

        rrdtool.graph(img,
            std_format_args,
            '--vertical-label', 'hPa',
            '--upper-limit', '1080',
            '--lower-limit', '950',
            '--alt-y-grid',
            '--units-exponent', '0',
            '--title', title,
            '--start', graph_start,
            defs, cdefs, plots
        )
    else:
        raise LookupError("invalid metric: ", metric)

def regen_chart(period, rrddir, imgdir, verbose):
    """
    Regenerate a series of charts for the given period
    @param period   The period whose graphs should be regenerated
    @param rrddir   Location of RRD files
    @param imgdir   Location of graph images
    @param verbose  If True, messages will be written to stdout
    """

    for area in ['inside', 'outside']:
        plot_chart(area, 'temp', period, rrddir, imgdir)
        plot_chart(area, 'pres', period, rrddir, imgdir)


def regenerate(rrddir, imgdir, mode, verbose):
    """
    Regenerate graphs according to the given mode
    @param rrddir   Location of RRD files
    @param imgdir   Location of graph images
    @param mode     One of: minute, hour, day
    @param verbose  If True, messages will be written to stdout
    """
    if mode == 'minute':
        regen_chart('day', rrddir, imgdir, verbose)
    elif mode == 'hour':
        regen_chart('week', rrddir, imgdir, verbose)
        regen_chart('month', rrddir, imgdir, verbose)
    elif mode == 'day':
        regen_chart('year', rrddir, imgdir, verbose)


if __name__ == '__main__':

    cgitb.enable()

    cgi = cgi.parse()

    if len(cgi.keys()) > 0:

        area = cgi['area'][0] if cgi.has_key('area') else None
        metric = cgi['metric'][0] if cgi.has_key('metric') else None
        period = cgi['period'][0] if cgi.has_key('period') else None

        ['inside', 'outside'].index(area)
        ['temp', 'pres'].index(metric)
        ['1d', '1w', '1m', '6m', '1y'].index(period)

        print "Content-Type: image/png"
        print
        plot_chart(area, metric, period, cfg.rrddir, None)

    else:
        p = argparse.ArgumentParser(description='Regenerate sensor graphs')
        p.add_argument('-v', '--verbose', action='store_true',
            help='verbose mode')
        p.add_argument('--rrddir',  default='.', required=True,
            help='location of RRD data files')
        p.add_argument('--imgdir',  default='.',
            help='directory to write graphs')
        p.add_argument('mode', choices=['minute','hour','day'],
            help='mode for regenerating graphs')
        args = p.parse_args()

        regenerate(args.rrddir, args.imgdir, args.mode, args.verbose)


