import json
import pigpio
import time
import datetime
import sys #python3 light.py on/off
import os
FILEPATH = os.path.dirname(os.path.abspath(__file__))

SIGNAL_FILE = f"{FILEPATH}/light.json"
SIGNAL_INDEX = f"light_{sys.argv[1]}"
FREQ = 38.0
GAP = 0.1 #[s]

def carrier(gpio, frequency, micros)->list[pigpio.pulse]:
	"""
	Generate carrier square wave.
	"""
	wf = []
	cycle = 1000.0 / frequency
	cycles = int(round(micros/cycle))
	on = int(round(cycle / 2.0))
	sofar = 0
	for c in range(cycles):
		target = int(round((c+1)*cycle))
		sofar += on
		off = target - sofar
		sofar += off
		wf.append(pigpio.pulse(1<<gpio, 0, on))
		wf.append(pigpio.pulse(0, 1<<gpio, off))
	return wf

pi = pigpio.pi()
try:
	with open(SIGNAL_FILE,"r") as sf:
		signal:list[int] = json.load(sf)[SIGNAL_INDEX]
	PIN = 19
	pi.set_mode(PIN, pigpio.OUTPUT)
	pi.wave_add_new()

	emit_time = time.time()

	wave:list = []
	marks_wid:dict[int] = {} #High区間
	spaces_wid:dict[int] = {} #Low区間
	for i, delay in enumerate(signal):
		if(i&0b1): #Low
			if delay not in spaces_wid:
				pi.wave_add_generic([pigpio.pulse(0,0,delay)])
				spaces_wid[delay] = pi.wave_create()
			wave.append(spaces_wid[delay])
		else: #High
			if delay not in marks_wid:
				wf = carrier(PIN, FREQ, delay)
				pi.wave_add_generic(wf)
				marks_wid[delay] = pi.wave_create()
			wave.append(marks_wid[delay])
	delay = emit_time - time.time()
	if delay>0.0:
		time.sleep(delay)

	pi.wave_chain(wave)
	while pi.wave_tx_busy():
		time.sleep(0.002)
	print(datetime.datetime.now().strftime("%y-%m-%d %H:%M:%S"), os.path.basename(__file__), f": Light {sys.argv[1]}")
except Exception as e:
	print(e)
finally:
	pi.wave_clear()
	pi.stop()