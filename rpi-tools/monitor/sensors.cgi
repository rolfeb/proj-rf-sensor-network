#!/usr/bin/env python
# -*- coding: utf_8 -*-
#
# Sensor report webpage
#

import os
import cgi
import urlparse
import urllib
import cgitb
import subprocess
import imp
import math

class Url(object):
    def __init__(self, url):
        """Construct from a string."""
        self.scheme, self.netloc, self.path, self.params, self.query, self.fragment = urlparse.urlparse(url)
        self.args = dict(cgi.parse_qsl(self.query))

    def __str__(self):
        """Turn back into a URL."""
        self.query = urllib.urlencode(self.args)
        return urlparse.urlunparse((self.scheme, self.netloc, self.path, self.params, self.query, self.fragment))


cgitb.enable()

QUERY = "/home/pi/sensors/query"

#
# Load sensor configuration
#
(file, path, desc) = imp.find_module("sensor-cfg", [ ".", "/etc", ])
cfg = imp.load_module("sensors", file, path, desc)

u = Url(os.environ['REQUEST_URI'])
if not u.args.has_key('period'):
    u.args['period'] = '1d'

p = subprocess.Popen([QUERY, "-c"], shell=False, stdout=subprocess.PIPE).stdout

print "Content-Type: text.html"
print
print """<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">
<html>
<head>
<meta name="viewport" content-"width=device-width,minimum-scale=1.0,maximum-scale=1.0"/>
<title>Sensors</title>
<link rel="stylesheet" type="text/css" href="/sensors/sensors.css">
</head>
<body>
<h1>Sensor readings</h1>
<div id="current1">
<div id="current2">
<table>
<tr>
<th class="id">Id</th>
<th class="station">Station</th>
<th class="temp">Temp</th>
<th class="pressure">Pressure</th>
<th class="status">Status</th>
</tr>"""

for line in p.readlines():
	line = line.rstrip()
	fields = line.split(",")
	(station, age, s1, s2, s3, s4, s5, junk) = fields

        if s1:
            temperature = float(s1) / 10

        if s2:
            pressure = float(s2) / 10

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

	print """<tr>
<td class="id">%s</td>
<td class="station">%s</td>
<td class="temp">%s</td>
<td class="pressure">%s</td>
<td class="status">%s</td>
</tr>""" % (
	station,
	cfg.sensors[station]['location'],
	("%.1f" % temperature if s1 else ""),
	("%.1f" % pressure_msl if s2 else ""),
	("offline" if (float(age) > 300) else "OK")
)

print """</table>
</div>
</div>
<br clear="all"/>
<div class="graph">
<div class="links">"""

called_period = u.args['period']

for other_period in ['1d', '1w', '1m', '6m', '1y']:
    if called_period == other_period:
        print '<span class="nolink">%s</span>' % other_period
    else:
        u.args['period'] = other_period
        print '<a href="%s">%s</a>' % (str(u), other_period)

print "</div>"

print '<img src="/cgi-bin/chart.py?area=inside&metric=temp&period=%s"</img>' % called_period

print """</div>
<br clear="all"/>

<div class="graph">
<div class="links">"""

for other_period in ['1d', '1w', '1m', '6m', '1y']:
    if called_period == other_period:
        print '<span class="nolink">%s</span>' % other_period
    else:
        u.args['period'] = other_period
        print '<a href="%s">%s</a>' % (str(u), other_period)

print "</div>"

print '<img src="/cgi-bin/chart.py?area=outside&metric=temp&period=%s"</img>' % called_period

print """</div>
<br clear="all"/>"""

print """</body>
</html>"""


