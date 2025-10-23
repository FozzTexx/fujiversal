##########################################################################
# Copyright 2024 by Chris Osborn <fozztexx@fozztexx.com>
#
# This file is part of paumaed.
#
#  paumaed is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  paumaed is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with paumaed. If not, see <http://www.gnu.org/licenses/>.
##########################################################################

from termcolor import colored

def hexdump(data, address=0, bytes_per_row=16, byte_format="{:02x}",
            highlight=None, color=None, on_color=None):
  chars_per_byte = len(byte_format.format(255))
  chars_per_addr = len(f"{address + len(data):x}")
  split_data = [data[x:x+bytes_per_row] for x in range(0, len(data), bytes_per_row)]
  for idx, row in enumerate(split_data):
    to_highlight = []
    if highlight:
      to_highlight = [x - idx * bytes_per_row for x in highlight
                      if idx * bytes_per_row <= x < (idx + 1) * bytes_per_row]
    as_ascii = [chr(x) if x >= ord(' ') and x < 127 else '.' for x in row]
    as_fmt = [byte_format.format(x) for x in row]
    for jdx in to_highlight:
      as_ascii[jdx] = colored(as_ascii[jdx], color, on_color=on_color, force_color=True)
      as_fmt[jdx] = colored(as_fmt[jdx], color, on_color=on_color, force_color=True)

    pad_count = bytes_per_row - len(row)
    blanks = " " * ((chars_per_byte + 1) * pad_count)
    line = " ".join(as_fmt) + blanks
    ascii = "".join(as_ascii)
    print(f"{idx * bytes_per_row + address:0{chars_per_addr}x}:", line, " ", ascii)
  return
