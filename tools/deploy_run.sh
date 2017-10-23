#! /bin/sh
scp car_remote_nRF905 pi@192.168.0.121:/home/pi/car_remote/tests/
ssh pi@192.168.0.121 "/home/pi/car_remote/tests/car_remote_nRF905"
