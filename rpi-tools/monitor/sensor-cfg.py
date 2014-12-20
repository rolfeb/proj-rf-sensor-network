#
# Define the list of known sensors
#

sensors = {
    '2':    { 'location': 'Lounge',
                'area': 'inside', 'sort': 15, 'temp': True },
    '3':    { 'location': 'Roof cavity (lower)',
                'area': 'outside', 'sort': 20, 'temp': True },
    '4':    { 'location': 'Roof cavity (upper)',
                'area': 'outside', 'sort': 21, 'temp': True },
    '10':   { 'location': 'Bedroom',
                'area': 'inside', 'sort': 2, 'temp': True },
    '21':   { 'location': 'Outside',
                'area': 'outside', 'sort': 1, 'temp': True, 'pres': True },
}

rrddir = '/home/pi/sensors'

