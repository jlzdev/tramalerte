#!/usr/bin/env python3
"""Capture les logs serie de l'ESP32 : serie.py [port] [secondes] [--reset]"""
import sys
import time

import serial

port = next((a for a in sys.argv[1:] if a.startswith('/dev/')), '/dev/ttyUSB0')
duree = next((int(a) for a in sys.argv[1:] if a.isdigit()), 30)
s = serial.Serial(port, 115200, timeout=1)
if '--reset' in sys.argv:
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)
debut = time.time()
while time.time() - debut < duree:
    ligne = s.readline()
    if ligne:
        sys.stdout.write(ligne.decode('utf-8', errors='replace'))
        sys.stdout.flush()
s.close()
