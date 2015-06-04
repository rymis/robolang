#!/usr/bin/env python

import sys, os

_file = None
def svg(filename):
    global _file
    if _file:
        _file.close()
    _file = open(filename, "wt")

def svg2png(nm):
    os.system("inkscape -z -e %s.png %s.svg" % (nm, nm))
    os.unlink(nm + ".svg")

def p(s):
    if _file:
        _file.write(s)
        _file.write('\n')
    else:
        sys.stdout.write(s)
        sys.stdout.write('\n')

def start(width, height):
    p("""<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg
   xmlns:dc="http://purl.org/dc/elements/1.1/"
   xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
   xmlns:svg="http://www.w3.org/2000/svg"
   xmlns="http://www.w3.org/2000/svg"
   width="%d"
   height="%d"
   viewBox="0 0 %d %d"
   version="1.1" >
  <g id="layer1">""" % (width, height, width, height))

def end():
    p("</g></svg>")

def ellipse(cx, cy, rx, ry, style, add = None):
    if not add:
        add = ""
    p("<ellipse style=\"%s\" cx=\"%f\" cy=\"%f\" rx=\"%f\" ry=\"%f\" %s />" % (style, cx, cy, rx, ry, add))

def rect(x, y, width, height, ry, style, add = None):
    if not add:
        add = ""
    p('<rect style="%s" x="%f" y="%f" width="%f" height="%f" ry="%f" %s />' % (style, x, y, width, height, ry, add))

def transform(rotate = None, scale_x = None, scale_y = None, translate_x = None, translate_y = None):
    s = ""
    if rotate:
        if isinstance(rotate, tuple):
            a = rotate[0]
            cx = rotate[1] if len(rotate) > 1 else 0.0
            cy = rotate[2] if len(rotate) > 2 else 0.0
            s = s + " rotate(%f %f %f)" % (a, cx, cy)
        else:
            s = s + " rotate(%f)" % rotate
    if scale_x or scale_y:
        if scale_x is None:
            scale_x = 1.0
        if scale_y is None:
            scale_y = 1.0
        s = s + " scale(%f %f)" % (scale_x, scale_y)
    if translate_x or translate_y:
        if translate_x is None:
            translate_x = 0.0
        if translate_y is None:
            translate_y = 0.0
        s = s + " translate(%f %f)" % (translate_x, translate_y)
    if s:
        return 'transform="%s"' % s
    else:
        return ""

def man(a1, a2, ah, scale_x):
    start(50, 100)

    translate_x = 0.0
    if scale_x < 0.96 or scale_x > 1.04:
        translate_x = 25 * (1.0 - scale_x)
    # Head:
    ellipse(25, 25, 16, 14, "fill:#000000", transform(rotate = (ah, 25, 25), scale_x = scale_x, translate_x = translate_x))
    # Body:
    ellipse(25, 55, 13, 17, "fill:#000000", transform(rotate = (-ah, 25, 55), scale_x = scale_x, translate_x = translate_x))

    # Eyes:
    ellipse(20, 25, 3, 2, "fill:#0000ff", transform(scale_x = scale_x, translate_x = translate_x))
    ellipse(30, 25, 3, 2, "fill:#0000ff", transform(scale_x = scale_x, translate_x = translate_x))

    # Leaps:
    ellipse(25, 35, 7, 1, "fill:#ff5555", transform(scale_x = scale_x, translate_x = translate_x))

    # Legs:
    rect(15, 65, 5, 25, 4, "fill:#000000", transform(scale_x = scale_x, rotate = (a1, 15, 65), translate_x = translate_x))
    rect(30, 65, 5, 25, 4, "fill:#000000", transform(scale_x = scale_x, rotate = (a2, 30, 65), translate_x = translate_x))

    # Hands:
    rect(10, 45, 5, 18, 2.5, "fill:#000000", transform(rotate = (a1, 20, 45), scale_x = scale_x, translate_x = translate_x))
    rect(35, 45, 5, 18, 2.5, "fill:#000000", transform(rotate = (a2, 45, 45), scale_x = scale_x, translate_x = translate_x))

    end()

# Walk:
for i in range(5):
    svg("walk-%d.svg" % i)
    man(0, -3.0 * i, i, 1.0)
for i in range(5):
    svg("walk-%d.svg" % (i + 5))
    man(3.0 * i, -15, (4 - i), 1.0)
for i in range(5):
    svg("walk-%d.svg" % (i + 10))
    man(15, -3.0 * (4 - i), -i, 1.0)
for i in range(5):
    svg("walk-%d.svg" % (i + 15))
    man(3.0 * (4 - i), 0, (i - 4), 1.0)

# Rotate:
for i in range(5):
    svg("rotate-%d.svg" % i)
    man(0, 0, 0, 0.2 * (5.0 - i))

for i in range(6):
    svg("rotate-%d.svg" % (i + 5))
    man(0, 0, 0, 0.2 * i if i > 0 else 0.1)

_file.close()
_file = None

for i in range(20):
    svg2png("walk-%d" % i)
for i in range(11):
    svg2png("rotate-%d" % i)

