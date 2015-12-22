#!/usr/bin/python

import getopt
import sys
import shutil
import os
import subprocess

GOOGLE_DRIVE_DIR = "/var/lib/google-drive"

def usage():
    print """\
usage: %s [options] file(s)
Options:
  -d | --directory              the directory to put it in (relative root) on google drive
""" % sys.argv[0]

try:
     opts, args = getopt.getopt(sys.argv[1:], "d:", ['directory'])
except getopt.GetoptError, err:
     print str(err)
     usage()
     sys.exit(2)

if len(args) < 1:
    usage();
    sys.exit(2)

directory = None
files = args
ret = 0

for opt in opts:
     k, v = opt
     if k == '-d' or k == '--directory':
         directory = str(v)

print "Uploading: ", files
for file in files:
    filename = os.path.basename( file )
    if directory != None:
        dir = GOOGLE_DRIVE_DIR + "/" + directory + "/"
        if not os.path.exists( dir ):
            os.makedirs( dir )
    else:
        dir = GOOGLE_DRIVE_DIR

    dest = dir + "/" + filename
    shutil.copyfile(file, dest)
    upload_ret = subprocess.call(["/usr/bin/drive","push","--no-prompt",dest])
    if upload_ret != 0:
        ret = upload_ret

    try:
        os.remove( dest )
    except Exception,err:
        print str(err)
    
sys.exit(ret)
