#!/usr/bin/env python

import argparse
import array
import struct
import serial

class MemoryRow:
    def __init__(self, addr, word_size):
        self.data = []
        self.address = addr
        self.word_size = word_size

    def store_word_at(self, location, data):
        if len(self.data) <= location:
            self.data += [0] * (location - len(self.data) + 1)
        self.data[location] = data

    def get_word_list(self):
        return self.data


class IntelHex:
    def __init__(self):
        self.memory = {}
        self._offset = 0

    def decode_record(self, s):
        s = s.strip(' \t\n\r')
        if s == '':
            return
        if not s.startswith(':'):
            raise Exception('Unexpected line')
        b = array.array('B', bytearray.fromhex(s[1:]))
        rec_len = b[0]
        rec_addr = b[1] * 0x100 + b[2] + self._offset
        rec_type = b[3]
        rec_crc = b[4 + rec_len]
        if sum(b) & 0xFF != 0:
            raise Exception('Checksum is wrong')
        if rec_type == 0:
            #print(rec_addr, b, sum(b))
            for i in range(0, rec_len):
                self.memory[rec_addr + i] = b[4 + i]
        elif rec_type == 1:
            return True #EOF
        elif rec_type == 4:
            off = b[4]*0x100 + b[5]
            self._offset = off * 0x10000
        else:
            raise Exception('Unimplemented')

    def from_file(self, path):
        is_eof = False
        with open(path, mode='r') as hexfile:
            lines = hexfile.readlines()
            for ln in lines:
                is_eof = self.decode_record(ln)
        if not is_eof:
            raise Exception('Did not see EOF')

    def to_rows(self, row_size, word_size):
        rows = {}
        curr_row = None
        rowsz_bytes = row_size * word_size
        for byteaddr in sorted(self.memory.keys()):
            rn = int(byteaddr / rowsz_bytes)
            row_start = rn * rowsz_bytes
            if rn in rows:
                curr_row = rows[rn]
            else:
                curr_row = MemoryRow(row_start, word_size)
                rows[rn] = curr_row
            if byteaddr % word_size == 0:
                idata = 0
                if word_size == 1:
                    idata = self.memory[byteaddr]
                elif word_size == 2:
                    idata = self.memory[byteaddr] + self.memory[byteaddr+1] * 0x100
                elif word_size == 4:
                    idata = self.memory[byteaddr] + self.memory[byteaddr+1] * 0x100 \
                    + self.memory[byteaddr+2] * 0x10000 + self.memory[byteaddr+3] * 0x1000000
                else:
                    raise "Irregular word size"
                curr_row.store_word_at(int((byteaddr - row_start) / word_size), idata)
        return [rows[key] for key in sorted(rows.keys())]


class PIC18K40ICSP:
    def __init__(self, serialport):
        self.ser = serialport

    def serial_send(self, buffer, resp_len = 1):
        self.ser.timeout = 1
        self.ser.write(buffer)
        ret = self.ser.read(resp_len)
        if not ret.endswith(b'a'):
            print(ret)
            raise 'Serial port communication error'
        return ret[:-1]

    def lvp_begin(self):
        self.serial_send(b'@ ')

    def lvp_end(self):
        self.serial_send(b'A ')

    def load_pc(self, addr):
        addrbuf = struct.pack('>I', addr)
        self.serial_send(b'H' + addrbuf)

    def print_current(self):
        rsp = self.serial_send(b'K', 3)
        v1 = struct.unpack('>H', rsp)
        print(hex(v1[0]))

    def erase_device(self):
        self.load_pc(0)
        self.serial_send(b'I')

    def program_row(self, row):
        command = b'B'
        command += struct.pack('>I', row.address)
        command += struct.pack('>H', len(row.get_word_list()) * 2)
        for word in row.get_word_list():
            command += struct.pack('<H', word)
        command += b' '
        self.serial_send(command)
        self.load_pc(row.address)
        self.serial_send(b'P')
        '''self.load_pc(row.address)
        for word in row.get_word_list()[:-1]:
            wordbuf = struct.pack('>H', word)
            self.serial_send(b'M' + wordbuf)
        wordbuf = struct.pack('>H', row.get_word_list()[-1])
        self.serial_send(b'N' + wordbuf)
        self.serial_send(b'P')'''

    def verify_row(self, row):
        self.load_pc(row.address)
        length = len(row.get_word_list())
        for i in range(0, length):
            readrsp = self.serial_send(b'K', 3)
            v1 = struct.unpack('>H', readrsp)
            if v1[0] != row.get_word_list()[i]:
                print('!', v1[0], row.get_word_list()[i])

    def program_one_word(self, addr, word):
        self.load_pc(addr)
        wordbuf = struct.pack('>H', word)
        self.serial_send(b'N' + wordbuf)
        self.serial_send(b'P')

        readrsp = self.serial_send(b'K', 3)
        v1 = struct.unpack('>H', readrsp)
        if v1[0] != word:
            raise "Verification error"

    def program_one_word_list(self, row):
        for i in range(0, len(row.get_word_list())):
            self.program_one_word(row.address + i*2, row.get_word_list()[i])

def main():
    parser = argparse.ArgumentParser(prog='Arduino PIC18K Programmer', usage=None, description=None)
    parser.add_argument('-P', '--port', dest='port')
    parser.add_argument('-d', '--download', dest='download')
    parser.add_argument('-i', '--info', dest='info', action='store_true')
    parser.add_argument('-t', '--test', dest='test', action='store_true')
    parser.add_argument('-v', '--verify', dest='verify', action='store_true')
    args = parser.parse_args()

    if not args.port:
        args.port = 'COM3'

    if args.test:
        ser = serial.Serial(args.port, 115200)
        ser.timeout = 1
        device = PIC18K40ICSP(ser)
        device.lvp_begin()
        device.load_pc(0x300000)
        device.print_current()
        device.lvp_end()

    elif args.info:
        ser = serial.Serial(args.port, 115200)
        ser.timeout = 1
        device = PIC18K40ICSP(ser)
        device.lvp_begin()
        device.load_pc(0x3FFFFE)
        print('Device ID:')
        device.print_current()
        device.lvp_end()

    elif args.download:
        ser = serial.Serial(args.port, 115200)
        ser.timeout = 1
        device = PIC18K40ICSP(ser)
        device.lvp_begin()
        device.erase_device()

        hf = IntelHex()
        hf.from_file(args.download)
        pending_rows = hf.to_rows(64, 2)
        for row in pending_rows:
            if row.address <= 0x20000:
                print('Address:', row.address, 'Data:', row.get_word_list())
                device.program_row(row)
                if args.verify:
                    device.verify_row(row)
            elif row.address == 0x200000:
                device.program_one_word_list(row)
            elif row.address == 0x300000:
                device.program_one_word_list(row)
            else:
                raise "Unrecognized segment"
        device.lvp_end()
        print('Finished!')

if __name__ == '__main__':
    main()
