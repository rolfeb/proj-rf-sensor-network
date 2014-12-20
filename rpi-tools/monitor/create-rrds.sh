#!/bin/sh
#
# Create RRD files
#

#
# RRAs for sensor monitoring (with 60 second polling)
#
#   1min resolution (1) for 1d (61x24=1464)
#   5min resolution (5) for 2w (12x24x7x2=4032)
#   1hr resolution (60) for 1y (24x366=8784)
#   1day resolution (60x24) for 5y (366x5=1830)
#
RRAS="RRA:AVERAGE:0.5:1:1464 RRA:AVERAGE:0.5:5:4032 RRA:AVERAGE:0.5:60:8784 RRA:AVERAGE:0.5:1440:1830"

ARGS="--step 60 --no-overwrite"

DS_TEMP="DS:temp:GAUGE:600:-10:50"
DS_PRES="DS:pres:GAUGE:600:900:1100"

rrdtool create station2.rrd $ARGS   $DS_TEMP            $RRAS
rrdtool create station3.rrd $ARGS   $DS_TEMP            $RRAS
rrdtool create station4.rrd $ARGS   $DS_TEMP            $RRAS
rrdtool create station10.rrd $ARGS  $DS_TEMP            $RRAS
rrdtool create station21.rrd $ARGS  $DS_TEMP $DS_PRES   $RRAS

