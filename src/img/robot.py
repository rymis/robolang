#!/usr/bin/env python

import os, sys, shutil

# Generate walking robot sprite:
mypath = None
robot_pov = None
robot_dir = None
xml_file = None
robot_options = None
last_image = 0

def gen_robot(body_rot, larm_rot, rarm_rot, lleg_rot, rleg_rot):
    global last_image

    opts = open(robot_options, "wt")
    opts.write("#declare LEFT_ARM_ANGLE  = %f;" % larm_rot)
    opts.write("#declare RIGHT_ARM_ANGLE = %f;" % rarm_rot)
    opts.write("#declare BODY_ANGLE      = %f;" % body_rot)
    opts.write("#declare LEFT_LEG_ANGLE  = %f;" % lleg_rot)
    opts.write("#declare RIGHT_LEG_ANGLE = %f;" % rleg_rot)
    opts.close()

    fnm = os.path.join(mypath, "robot", "frame%03d" % last_image)
    snm = "robot/frame%03d.png" % last_image
    os.system("povray -d '+O%s-tmp' +H500 +W500 +Irobot.pov" % fnm)
    os.system("convert '%s-tmp.png' -transparent black '%s.png'" % (fnm, fnm))
    os.unlink('%s-tmp.png' % fnm)
    last_image += 1

    os.unlink(robot_options)
    return snm

def frame(nm):
    xml_file.write('\t\t<frame img="%s"/>\n' % nm)

def gen_walk(nm, brot):
    xml_file.write('\t<mode name="%s">\n' % nm)
    for i in range(4):
        frame(gen_robot(brot, i * 10.0, -i * 10.0, -i * 12.0, i * 12.0))

    for i in range(3):
        frame(gen_robot(brot, (2 - i) * 10.0, -(2 - i) * 10.0, -(2 - i) * 12.0, (2 - i) * 12.0))

    for i in range(4):
        frame(gen_robot(brot, -i * 10.0, i * 10.0, i * 12.0, -i * 12.0))

    for i in range(2):
        frame(gen_robot(brot, -(2 - i) * 10.0, (2 - i) * 10.0, (2 - i) * 12.0, -(2 - i) * 12.0))
    xml_file.write('\t</mode>\n')

def gen_rotate(nm, a1, a2):
    d = (a2 - a1) / 10.0
    xml_file.write('\t<mode name="%s">\n' % nm)
    for i in range(11):
        j = (i % 3) - 1
        frame(gen_robot(a1 + i * d, j * 10, -j * 10, -j * 12, j * 12))
    xml_file.write('\t</mode>\n')

def gen_check(nm, brot):
    xml_file.write('\t<mode name="%s">\n' % nm)
    for i in range(10):
        frame(gen_robot(brot, 0, i * 10, 0, 0))
    for i in range(9):
        frame(gen_robot(brot, 0, (8 - i) * 10, 0, 0))
    xml_file.write('\t</mode>\n')

def main():
    global mypath, robot_pov, robot_dir, xml_file, robot_options
    mypath = os.path.abspath(os.path.dirname(sys.argv[0]))
    robot_pov = os.path.join(mypath, "robot.pov")
    if not os.path.exists(robot_pov):
        print("Error: robot.pov is not found")
    robot_dir = os.path.join(mypath, "robot")
    if not os.path.isdir(robot_dir):
        os.mkdir(robot_dir)
    robot_options = os.path.join(mypath, "robot_options.inc")
    robot_ini = os.path.join(mypath, "robot.xml")
    xml_file = open(robot_ini, "wt")
    xml_file.write('<?xml version="1.0" encoding="utf-8"?>\n')
    xml_file.write("<!-- This file is generated automatically from robot.pov -->\n\n")
    xml_file.write("<sprite>\n")

    gen_walk("walk_d", 0)
    gen_walk("walk_u", 180)
    gen_walk("walk_r", -90)
    gen_walk("walk_l", 90)

    gen_rotate("rotate_dr", 0, -90)
    gen_rotate("rotate_ru", -90, -180)
    gen_rotate("rotate_ul", 180, 90)
    gen_rotate("rotate_ld", 90, 0)

    gen_rotate("rotate_dl", 0, 90)
    gen_rotate("rotate_lu", 90, 180)
    gen_rotate("rotate_ur", -180, -90)
    gen_rotate("rotate_rd", -90, 0)

    gen_check("check_d", 0)
    gen_check("check_r", -90)
    gen_check("check_l", 90)
    gen_check("check_u", 180)

    xml_file.write("</sprite>\n")
    xml_file.close()

if __name__ == '__main__':
    main()
