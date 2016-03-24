Logging Configuration
---------------------

Configuration files for monitoring logs sent over from the RPi sensord.

1. Log forwarding

The sensord logs important events via the local1 syslog facility. These
are relayed to a log-host by adding the following file on the local
host:

/etc/rsyslog.d/sensord.conf:

    #
    # Send log messages from sensord to the loghost (moonbase)
    #
    local1.*        @@loghost

On the logging host, adding the following to /etc/rsyslog.conf and
restart the service:

    local1.*        /var/log/sensord

2. Log monitoring

To monitor the logs for important events and include them in the daily
logwatch report, copy the files in the logwatch directory to their
corresponding location under /etc/logwatch:

    logwatch/conf/services/sensord.conf
    logwatch/conf/logfiles/sensord.conf
    logwatch/scripts/services/sensord

3. Log rotation

To have the sensord logs rotated along with the other syslog files,
add the foillowing line to /etc/logrotate.d/syslog:

    /var/log/sensord
