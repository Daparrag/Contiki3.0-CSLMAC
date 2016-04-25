#!/usr/bin/env python

import os
import subprocess
import shutil

configList = []

DURATION = 30
ITERATIONS = 20

BUID_BINARIES = False
SCHEDULE_JOBS = True

#PROBING ?= 1
#SQUAREDETX ?= 1
#CONSERVATIVE ?= 1
#RSSI_BASED_ETX ?= 1

configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 1, 'squaredetx': 1, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 1, 'squaredetx': 1, 'rssibased': 1})

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'conservative': 0, 'squaredetx': 1, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'conservative': 0, 'squaredetx': 1, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 0, 'squaredetx': 1, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 0, 'squaredetx': 1, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 1, 'squaredetx': 0, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 1, 'squaredetx': 0, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 1, 'squaredetx': 1, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 1, 'squaredetx': 1, 'rssibased': 0})

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 0, 'squaredetx': 0, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 0, 'squaredetx': 0, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 1, 'conservative': 1, 'squaredetx': 0, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 1, 'conservative': 1, 'squaredetx': 0, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'conservative': 0, 'squaredetx': 1, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'conservative': 0, 'squaredetx': 1, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'conservative': 0, 'squaredetx': 0, 'rssibased': 1})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'conservative': 0, 'squaredetx': 0, 'rssibased': 1})

#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 2, 'probing': 0, 'conservative': 0, 'squaredetx': 0, 'rssibased': 0})
#configList.append({'app': 'app-rpl-ping-pong', 'rpl_mode': 1, 'probing': 0, 'conservative': 0, 'squaredetx': 0, 'rssibased': 0})

if BUID_BINARIES:
  # get current git hash
  gitHash = subprocess.check_output(["git", "log", "-1", "--format='%h'"])[1:-2]

  for config in configList:

    cleanCmd = ["make",
    			"TARGET=jn516x",
                "clean"
                ]        

    buildCmd = ["make",
                "TARGET=jn516x",
                "DEP=5",
                "RPL_CONFIG=%s" %(config['rpl_mode']),
                "PROBING=%s" %(config['probing']),
                "CONSERVATIVE=%s" %(config['conservative']),
                "SQUAREDETX=%s" %(config['squaredetx']),
                "RSSI_BASED_ETX=%s" %(config['rssibased']),
                "%s.jn516x.bin" %(config['app']),
                ]
                    
    newFileName = "down_rpl%d_prb%d_cons%d_squ%d_rssi%d.jn516x.bin" %(config['rpl_mode'], config['probing'], config['conservative'], config['squaredetx'], config['rssibased'])

    print "Generating file %s" %newFileName

    print "Cleaning"
    subprocess.check_output(cleanCmd)

    try:
        print "Compiling"
        subprocess.check_output(buildCmd)
        shutil.copyfile("%s.jn516x.bin" %(config['app']), newFileName)
        print "scp %s simonduq@pitestbed.sics.se:." %(newFileName)
        os.system("scp %s simonduq@pitestbed.sics.se:." %(newFileName))
    except Exception as e:                
        print "Build failed", e
            
  subprocess.check_output(cleanCmd)
  print "Done"

if SCHEDULE_JOBS:
  print "Creating jobs"

  for i in range(0, ITERATIONS):
	for config in configList:
		fileName = "down_rpl%d_prb%d_cons%d_squ%d_rssi%d.jn516x.bin" %(config['rpl_mode'], config['probing'], config['conservative'], config['squaredetx'], config['rssibased'])
		print "%d: creating %s" %(i, fileName)
		testbedCommand = "testbed.py create --copy-from %s --duration=%d --platform=jn516x" %(fileName, DURATION)
		os.system("ssh simonduq@pitestbed.sics.se '%s'" %(testbedCommand))
