#!/usr/bin/env python

import os
import sys
import re
import fileinput
import math
import parseLogs

def extract(dir):
    
    print "Looping over all experiments"
    for file in os.listdir(dir):
        path = (os.path.join(dir, file))
        if not os.path.exists(os.path.join(path, 'log.txt')):
        	continue
        print path,
        sys.stdout.flush()
        #if os.path.exists(os.path.join(path, 'ongoing')):
        #	print " is ongoing"
        #	continue
        if os.path.exists(os.path.join(path, '../.started')) and not os.path.exists(os.path.join(path, '../.stopped')):
        	print " is ongoing"
        	continue
        if os.path.exists(os.path.join(path, 'summary.txt')):
       	    print " summary already done."
            continue
        #if os.path.exists(os.path.join(path, 'plots/allplots.pdf')):
         #   print " plot already done."
          #  continue

        print " extracting data...",
        sys.stdout.flush() 
        os.system("python extractFromTrace.py %s > /dev/null" %path)
        
        print " generating plots...",
        sys.stdout.flush()
        os.system("python generateSummaryPlots.py %s > /dev/null" %path)
        
        print " done."

if len(sys.argv) < 2:
    dir = '.'
else:
    dir = sys.argv[1].rstrip('/')
extract(dir)
