#!/usr/bin/env python3
import argparse
import sys
import serial
import tty, termios
import select
import time
from hexdump import hexdump

EC_LEN = 32
DATA_LEN = 307

def build_argparser():
  parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument("serial", help="serial device")
  parser.add_argument("bps", default=9600, type=int, help="baud rate")
  parser.add_argument("--flag", action="store_true", help="flag to do something")
  return parser

def block_test(ser, data, delay):
  print("Delay", delay)
  ser.timeout = 0.01
  read_back = bytearray()
  begin = time.time()
  for b in data:
    ser.write(bytes([b]))
    v = ser.read()
    c = v[0] if v else None
    read_back += v
    # if b != c:
    #   print(b, c)
  end = time.time()

  print(f"Wrote: {len(data)} Read: {len(read_back)}")
  # print(data)
  # print(read_back)
  count = 0
  for a, b in zip(data, read_back):
    if a != b:
      #print(a, b)
      count += 1
  print("Mismatch:", count)
  print("CPS:", len(read_back) / (end - begin))
  return read_back == data

def main():
  args = build_argparser().parse_args()

  cps = max_cps = args.bps / 10
  seconds_per_char = 1 / max_cps
  ser = serial.Serial(args.serial, baudrate=args.bps, rtscts=True)

  orig_settings = termios.tcgetattr(sys.stdin)
  tty.setcbreak(sys.stdin)

  test_data = bytes([x % 256 for x in range(DATA_LEN)])

  recv_data = []
  sent_data = []
  last_recv = 0
  while True:
    if select.select([sys.stdin, ], [], [], 0.0)[0]:
      x = sys.stdin.read(1)[0]
      if x == chr(27):
        break
      elif x == '@' or x == '#':
        print("SENDING TEST DATA")
        delay = 1 / cps # - seconds_per_char
        delay /= 16
        begin = time.time()
        if x == '@':
          for b in test_data:
            ser.write(bytes([b]))
            time.sleep(delay)
        else:
          ser.write(test_data)
        ser.flush()
        end = time.time()
        print("CPS:", len(test_data) / (end - begin))
        sent_data = test_data
      # elif x == '|':
      #   delay = 1 / cps # - seconds_per_char
      #   success = block_test(ser, test_data, delay)
      #   print("Result:", success, cps)
      #   #ser.write(test_data)
      #   # for b in test_data:
      #   #   ser.write(bytes([b]))
      #   #   time.sleep(0.0001)
      # elif x == '+':
      #   cps += (max_cps - cps) / 2
      # elif x == '-':
      #   cps /= 2
      else:
        print("Sending", x)
        ser.write(x.encode("UTF-8"))
        sent_data.append(ord(x))

    if ser.in_waiting:
      recv_data.extend(ser.read())
      last_recv = time.time()

    if recv_data:
      now = time.time()
      delta = now - last_recv
      if delta > 1 / cps * 10:
        print(f"Received {len(recv_data)} of {len(sent_data)}")
        mismatch = [idx for idx, (a, b) in enumerate(zip(sent_data, recv_data)) if a != b]
        hexdump(recv_data, highlight=mismatch, color="white", on_color="on_red")
        recv_data = []
        sent_data = []

  print("Quitting")
  termios.tcsetattr(sys.stdin, termios.TCSADRAIN, orig_settings)
  return

if __name__ == '__main__':
  exit(main() or 0)
