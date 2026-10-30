#!/usr/bin/env python3
"""One concrete Q4 miss for the clip=10 sine/plane scenario on the host.
This compares a framebuffer and a sign-changing sine bracket; it is not a
proof that Q8 or any numerical search is correct for all rays/views.
"""
import math
import struct
import sys

q4 = open(sys.argv[1], 'rb').read()
q8 = open(sys.argv[2], 'rb').read()
width, height = 396, 224
assert len(q4) == len(q8) == width * height * 2
p4 = struct.unpack('=' + str(width * height) + 'H', q4)
p8 = struct.unpack('=' + str(width * height) + 'H', q8)

def slot(pixel):
    if pixel == 0xffff:
        return -1
    return 1 if (pixel >> 11) & 31 > (pixel & 31) else 0

changed = surface_changed = 0
for y in range(19, 200):
    for x in range(5, 391):
        a, b = p4[y * width + x], p8[y * width + x]
        changed += (a != b)
        surface_changed += (slot(a) != slot(b))

# For this ray, Q4 shows the plane; Q8 shows the sine surface.
x, y = 358, 137
assert slot(p4[y * width + x]) == 1 and slot(p8[y * width + x]) == 0
pitch, yaw, radius = .55, .75, 10.0
distance = 2.8 * min(radius, 3)
ox = distance * math.cos(pitch) * math.sin(yaw)
oy = distance * math.cos(pitch) * math.cos(yaw)
oz = distance * math.sin(pitch)
h = (x - 197.5) / (72 * 2.8)
v = (109 - y) / (72 * 2.8)
ry = -math.sin(pitch) * v - math.cos(pitch)
rz = math.cos(pitch) * v - math.sin(pitch)
dx = radius * (math.cos(yaw) * h + math.sin(yaw) * ry)
dy = radius * (-math.sin(yaw) * h + math.cos(yaw) * ry)
dz = radius * rz
plane_t = -(oz - ox + oy) / (dz - dx + dy)
def sine(t):
    return math.sin(ox + dx*t) + math.sin(oy + dy*t) + math.sin(oz + dz*t)
assert sine(.2484) < 0 < sine(.2485) and .2485 < plane_t
print('Q4/Q8 host viewport: %d pixel colors differ; %d change visible surface/background.' % (changed, surface_changed))
print('At (358,137), Q4 shows the plane although a sine root is bracketed before it. Default view only.')
