#!/usr/bin/env python
# -*- coding: utf_8 -*-
#

import sys
import argparse
import rrdtool
import imp
import subprocess

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
    (id, age, temp, pressure, count, light, humidity, junk) = f

    age = int(age)

    rrdfile = "%s/station%s.rrd" % (cfg.rrddir, id)

    metrics = []
    values = []

    if cfg.sensors[id]['temp']:
        metrics.append('temp')
        if temp != '':
            temp = float(temp) / 10
            values.append(str(temp))
        else:
            values.append('U')
    elif cfg.sensors[id]['pres']:
        metrics.append('pres')
        if pres != '':
            pressure = float(pressure) / 10
            values.append(str(pressure))
        else:
            values.append('U')

    metric_list = ':'.join(metrics)
    value_list = ':'.join(values)

    print \
            rrdfile, \
            '--template', metric_list, \
            '--', \
            '%d:%s' % (-age, value_list)
