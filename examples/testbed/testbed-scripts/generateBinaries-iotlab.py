#!/usr/bin/env python

import os
import subprocess
import shutil

configList = []

DEP = 6
TARGET = "iotlab-m3"
TARGET_BIN = "iotlab-m3"

DURATION = 10
ITERATIONS = 10
SSH_SERVER = "duquenno@grenoble.iot-lab.info"

BUID_BINARIES = True
SCHEDULE_JOBS = True

configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 1, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 1, 'daoack': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 1, 'daoack': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 1, 'daoack': 1})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 0, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'freshonly': 1, 'squaredetx': 1, 'rssibased': 0, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'freshonly': 1, 'squaredetx': 0, 'rssibased': 1, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'freshonly': 1, 'squaredetx': 0, 'rssibased': 1, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'freshonly': 0, 'squaredetx': 1, 'rssibased': 1, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'freshonly': 0, 'squaredetx': 1, 'rssibased': 1, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'freshonly': 0, 'squaredetx': 0, 'rssibased': 0, 'daoack': 0})
configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'freshonly': 0, 'squaredetx': 0, 'rssibased': 0, 'daoack': 0})

def getFileName(config):
  return "down_rpl%d_prb%d_cons%d_squ%d_rssi%d_daoack%d.%s" %(config['rpl_mode'], config['probing'], config['freshonly'], config['squaredetx'], config['rssibased'], config['daoack'], TARGET_BIN)

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
                "DEP=%d"%(DEP),
                "RPL_CONFIG=%s" %(config['rpl_mode']),
                "PROBING=%s" %(config['probing']),
                "FRESHONLY=%s" %(config['freshonly']),
                "SQUAREDETX=%s" %(config['squaredetx']),
                "DAOACK=%s" %(config['daoack']),
                "RSSI_BASED_ETX=%s" %(config['rssibased']),
                "%s.%s" %(config['app'], TARGET_BIN),
                ]
                    
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
