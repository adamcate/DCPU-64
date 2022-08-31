import struct

def float_to_hex(f):
    return hex(struct.unpack('<I', struct.pack('<f', f))[0])

def hex_to_float(h):
    return struct.unpack('<f', struct.pack('<I', h))[0]

def range4(a, b):
    w = b - a
    return [a, a+w/4, a+w/2, b-w/4, b]

def range3(a, b):
    w = b - a
    return [a, a+w/3, b-w/3, b]
