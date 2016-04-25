#!/usr/bin/env python

import os
import sys
import re
import fileinput
import math
#from pylab import *
#import pygraphviz as pgv
from sets import Set

def main():
    dir = sys.argv[1].rstrip('/')
    allLines = dict()
    nodeIdMap = {}
    destFile = open(os.path.join(dir, "log.txt"), 'w')
    for fileName in os.listdir(dir):
        file = os.path.join(dir, fileName)
        print "\nProcessing %s" %(file)        
                        
        lineOut = []
        timeStamp = []
        nodeId = -1        
        for line in open(file, 'r').readlines():
            lineSplit = line.split(None, 1)
            if len(lineSplit) != 2:
                continue
            try:    
                timeStamp = int(lineSplit[0])
            except:
                continue
                pass
            log = lineSplit[1]
            if not timeStamp in allLines:
                allLines[timeStamp] = []    
            allLines[timeStamp].append({'file': fileName, 'line': log})
            # get nodeId
            if nodeId == -1:
                res = re.compile('^Duty Cycle: \[(\d+) \d+\]').match(log)
                if res:
                    try:
                        nodeId = int(res.group(1))
                    except:
                        pass
                    if nodeId:
                        nodeIdMap[fileName] = nodeId
                        print 'found nodeid: %d' %(nodeId)
                        
    for timeStamp in sorted(allLines.keys()):
        for log in allLines[timeStamp]:
            file = log['file']
            line = log['line']
            if file in nodeIdMap:
                nodeId = nodeIdMap[file]
                destFile.write("%016d\tID:%d\t%s"%(timeStamp, nodeId, line))
                    

# execute main
main()
