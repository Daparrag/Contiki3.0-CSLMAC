#!/usr/bin/env python

import os
import subprocess
import shutil

configList = []

DEP = 6
TARGET = "iotlab-m3"
TARGET_BIN = "iotlab-m3"

DURATION = 60
ITERATIONS = 4
SSH_SERVER = "duquenno@grenoble.iot-lab.info"

BUID_BINARIES = False
SCHEDULE_JOBS = True

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'rssibased': 1, 'squaredetx': 1, 'smartdup': 1}) #12
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'rssibased': 1, 'squaredetx': 1, 'smartdup': 1}) #10

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'rssibased': 1, 'squaredetx': 0, 'smartdup': 1}) #16
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'rssibased': 1, 'squaredetx': 0, 'smartdup': 1}) #10

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'rssibased': 1, 'squaredetx': 0, 'smartdup': 0}) #10
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'rssibased': 1, 'squaredetx': 0, 'smartdup': 0}) #12

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'rssibased': 0, 'squaredetx': 0, 'smartdup': 0}) #10
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'rssibased': 0, 'squaredetx': 0, 'smartdup': 0}) #10

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'channels': 4, 'rtx':  8}) #12
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'channels': 4, 'rtx': 16}) #12
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'channels': 4, 'rtx': 32}) #12

configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'squaredetx': 0, 'channels': 4, 'rtx':  8}) #8
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'squaredetx': 0, 'channels': 4, 'rtx': 16}) #8
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'squaredetx': 0, 'channels': 4, 'rtx': 32}) #8

def getFileName(config):
    name = "do4s"
    if 'rpl_mode' in config:
        name += "_rpl%d"%(config['rpl_mode'])
    if 'probing' in config:
        name += "_prb%d"%(config['probing'])
    if 'freshonly' in config:
        name += "_fresh%d"%(config['freshonly'])
    if 'squaredetx' in config:
        name += "_squ%d"%(config['squaredetx'])
    if 'daoack' in config:
        name += "_daoack%d"%(config['daoack'])
    if 'rssibased' in config:
        name += "_rssi%d"%(config['rssibased'])
    if 'channels' in config:
        name += "_ch%d"%(config['channels'])
    if 'rtx' in config:
        name += "_rtx%d"%(config['rtx'])
    if 'smartdup' in config:
        name += "_smartdup%d"%(config['smartdup'])
    if 'fixloop' in config:
        name += "_fixloop%d"%(config['fixloop'])
    name += ".iotlab-m3"
    return name

if BUID_BINARIES:
  # get current git hash
  gitHash = subprocess.check_output(["git", "log", "-1", "--format='%h'"])[1:-2]

  for config in configList:

    cleanCmd = ["make",
    			"TARGET=%s"%(TARGET),
                "clean"
                ]

    buildCmd = ["make",
                "TARGET=%s"%(TARGET),
                "DEP=%d"%(DEP)
                ]

    if 'rpl_mode' in config:
        buildCmd += ["RPL_CONFIG=%s" %(config['rpl_mode'])]
    if 'probing' in config:
        buildCmd += ["PROBING=%s" %(config['probing'])]
    if 'freshonly' in config:
        buildCmd += ["FRESHONLY=%s" %(config['freshonly'])]
    if 'squaredetx' in config:
        buildCmd += ["SQUAREDETX=%s" %(config['squaredetx'])]
    if 'daoack' in config:
        buildCmd += ["DAOACK=%s" %(config['daoack'])]
    if 'rssibased' in config:
        buildCmd += ["RSSI_BASED_ETX=%s" %(config['rssibased'])]
    if 'channels' in config:
        buildCmd += ["CHANNELS=%s" %(config['channels'])]
    if 'rtx' in config:
        buildCmd += ["RTX=%s" %(config['rtx'])]
    if 'smartdup' in config:
        buildCmd += ["SMARTDUP=%s" %(config['smartdup'])]
    if 'fixloop' in config:
        buildCmd += ["FIXLOOP=%s" %(config['fixloop'])]
    
    
    buildCmd += ["%s.%s" %(config['app'], TARGET_BIN)]
                        
    newFileName = getFileName(config)

    print "Generating file %s" %newFileName

    print "Cleaning"
    subprocess.check_output(cleanCmd)

    try:
        print "Compiling"
        subprocess.check_output(buildCmd)
        shutil.copyfile("%s.%s" %(config['app'], TARGET_BIN), newFileName)
        print "scp %s %s:." %(newFileName, SSH_SERVER)
        os.system("scp %s %s:." %(newFileName, SSH_SERVER))
    except Exception as e:                
        print "Build failed", e
            
  subprocess.check_output(cleanCmd)
  print "Done"

if SCHEDULE_JOBS:
  print "Creating jobs"

  for i in range(0, ITERATIONS):
      for config in configList:
        fileName = getFileName(config)
        print "%d: creating %s" %(i, fileName)
        testbedCommand = "testbed.py create --copy-from %s --duration=%d" %(fileName, DURATION)
        os.system("ssh %s '%s'" %(SSH_SERVER, testbedCommand))
