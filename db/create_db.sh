#!/bin/sh

mysqladmin -f drop sensors
mysqladmin -u root -p create sensors
