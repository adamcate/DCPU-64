def coords(a, b):
    j = b * 4
    c = a * 8
    for i in range(256):
        b1 = i // 16
        z1 = i % 16
        x1 = b1 // 4
        y1 = z1 // 2
        z2 = z1 % 2
        b2 = b1 % 4
        z3 = z2 * 4
        z4 = z3 + b2
        #print(f'{i=} x={x1} y={y1} z={z4}')
        x2 = x1 + j
        x3 = x2 * 8
        y2 = y1 + c
        x4 = x3 + y2
        print(f'{i=} x={x1} y={y1} z={z4} / x={x4} z={z4}')

if __name__ == '__main__':
    coords(0, 0)
