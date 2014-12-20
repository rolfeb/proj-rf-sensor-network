#!/usr/bin/env python
# -*- coding: utf_8 -*-
#

import sys
import argparse
import rrdtool
import imp
import subprocess
import math

#
# Load sensor configuration
#
(file, path, desc) = imp.find_module("sensor-cfg",
    [ ".", "/etc", ])
cfg = imp.load_module("sensors", file, path, desc)

cmd = "/home/pi/sensors/query -c"

p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)

for l in p.stdout.readlines():

    f = l.strip().split(',')

    print f

    (id, age, temperature, pressure, count, light, humidity, junk) = f

    age = int(age)

    if temperature:
        temperature = float(temperature) / 10

    if pressure:
        pressure = float(pressure) / 10

        #
        # Convert to sea level pressure.
        # http://hyperphysics.phy-astr.gsu.edu/hbase/kinetic/barfor.html
        #
        m = 29.0 * 1.66054e-27      # 29 amu
        g = 9.8                     # G
        h = 210                     # height above sea level
        k = 1.38066e-23             # Boltzmann constant
        T = 273.15 + temperature    # temperature in Kelvin

        pressure_msl = pressure / math.exp(-m * g * h / k / T);

    rrdfile = "%s/station%s.rrd" % (cfg.rrddir, id)

    metrics = []
    values = []

    if cfg.sensors[id].has_key('temp') and cfg.sensors[id]['temp']:
        metrics.append('temp')
        if temperature != '':
            values.append(str(temperature))
        else:
            values.append('U')

    if cfg.sensors[id].has_key('pres') and cfg.sensors[id]['pres']:
        metrics.append('pres')
        if pressure != '':
            values.append(str(pressure_msl))
        else:
            values.append('U')

    metric_list = ':'.join(metrics)
    value_list = ':'.join(values)

    print rrdfile, age, metric_list, value_list

    try:
        rrdtool.update(
            rrdfile,
            '--template', metric_list,
            '--',
            '%d:%s' % (-age, value_list)
        )
    except:
        pass    # ignore error, probably attempt to add duplicate value
