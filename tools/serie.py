#!/usr/bin/env python3
"""Capture les logs serie de l'ESP32 sans le redemarrer : serie.py [port] [secondes] [--reset]

Les lignes DTR/RTS sont laissees au repos a l'ouverture et a la fermeture : sur un DevKit,
les basculer redemarre la carte (et peut laisser l'UART repeter ses derniers octets).
--reset redemarre volontairement la carte par la ligne RTS avant la capture.
"""
import sys
import time

import serial

port = next((a for a in sys.argv[1:] if a.startswith('/dev/')), '/dev/ttyUSB0')
duree = next((int(a) for a in sys.argv[1:] if a.isdigit()), 30)
s = serial.Serial()
s.port = port
s.baudrate = 115200
s.timeout = 1
s.dtr = False
s.rts = False
s.open()
if '--reset' in sys.argv:
    s.rts = True
    time.sleep(0.1)
    s.rts = False
debut = time.time()
while time.time() - debut < duree:
    ligne = s.readline()
    if ligne:
        sys.stdout.write(ligne.decode('utf-8', errors='replace'))
        sys.stdout.flush()
s.close()
