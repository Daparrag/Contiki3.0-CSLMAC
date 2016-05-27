#!/usr/bin/env python

import os
import re
import fileinput
import math
from sets import Set
from pylab import *
from collections import OrderedDict

def doConvert():
    global appDataStats, hopDataStats, receivedList, nodeState, timeline

    allData = []
    baseTime = None
    lastPrintedTime = 0
    t_us = None
        
    for line in sys.stdin.readlines():
        log = None
        module = None
        # match time, id, module, log; The common format for all log lines
        res = re.compile('^(\d+)\.(\d+);(.*);(.*)$').match(line)
        if res:
            t_sec = int(res.group(1))
            t_us = t_sec * 1000000 + int(res.group(2))
            id = res.group(3)
            log = res.group(4)
                        
            if not baseTime:
                baseTime = t_us
            
            t_us -= baseTime
            t_ms = t_us / 1000.
            t_s = t_ms / 1000.
            t_m = t_s / 60.
            
            print "%3u:%02u.%03u\t%s;%s" %(t_m, t_s%60, t_ms%1000, id, log)

def main():
    doConvert()
    
main()
