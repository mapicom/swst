import struct
import signal
import serial
import time
import datetime
import pytz
import win32api
import sys
from threading import Thread
from serial.tools import list_ports

BAUD_RATE = 115200

tz = pytz.timezone("UTC")
local_tz = pytz.timezone("Asia/Novokuznetsk")

usage_port_userfriendly = ""
terminating = False

def inputPort():
    global usage_port_userfriendly
    usage_port_userfriendly = input("Select port what you now: ")
    return usage_port_userfriendly

print("Available ports:")
i = 1
ports = list_ports.comports()
for port in ports:
    print("[{}] {}: {}".format(i, port.name, port.description))
    i += 1

while inputPort() == "":
    pass

usage_port_int = 0
try:
    usage_port_int = int(usage_port_userfriendly, 10)
except:
    print("Invalid port (1)")
    exit(1)

if usage_port_int > len(ports) or usage_port_int < 1:
    print("Invalid port (2)")
    exit(1)

usage_port = ports[usage_port_int-1]

print("Connecting to {}...".format(usage_port.device))

ser = 0
try:
    ser = serial.Serial(usage_port.device, BAUD_RATE, timeout=8.0)
except:
    print("Failed.");
    exit(1)

print("Connected. Starting listening...")
def checker_dance():
    global ser
    global terminating
    while True:
        if terminating:
            print("Closing port...")
            ser.close()
            break
        buf = ser.read(13)
        if buf[0:4] == b"SwSt":
            second = struct.unpack('B', buf[4:5])[0]
            minute = struct.unpack('B', buf[5:6])[0]
            hour = struct.unpack('B', buf[6:7])[0]
            day = struct.unpack('B', buf[7:8])[0]
            month = struct.unpack('B', buf[8:9])[0]
            year = struct.unpack('H', buf[9:11])[0]
            txid = struct.unpack('B', buf[12:])[0]
            date_time = datetime.datetime(year, month, day, hour, minute, second, tzinfo=tz)
            unix = int(date_time.timestamp())
            #date_time = date_time.astimezone(local_tz)
            if second == 0:
                try:
                    win32api.SetSystemTime(date_time.year, date_time.month, date_time.weekday(), date_time.day, date_time.hour, date_time.minute, date_time.second, 0)
                    print("Set system date.")
                except:
                    print("Failed to set system time.")
            print("{:02d}:{:02d}:{:02d} {:02d}-{:02d}-{} | Unix: {} | TXID: {}".format(hour, minute, second, day, month, year, unix, txid))

def signal_handler(sig, frame):
    global terminating
    global ser
    print("Signal received, exiting..")
    terminating = True
    while ser.is_open:
        time.sleep(1)
    exit(0)
signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)
t = Thread(target=checker_dance)
t.start()

while True:
    time.sleep(1)
