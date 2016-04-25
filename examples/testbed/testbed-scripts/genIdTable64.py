#!/usr/bin/env python

import re
import fileinput
import math
from pylab import *

idMacMap = {}
file = sys.argv[1]

def parseLine(line):
    #res = re.compile('^(\d+)\\tID:(\d+)\\t(.*)$').match(line)
    #if res:
    #    return int(res.group(1)), int(res.group(2)), res.group(3)
    res = re.compile('^(\d+)\.(\d+);m3-(\d+);(.*)$').match(line)
    if res:
        return int(res.group(1)) * 1000000 + int(res.group(2)), int(res.group(3)), res.group(4)
    return None, None, None

for line in open(file, 'r').readlines():
    # match time, id, module, log; The common format of any line
    time, id, log = parseLine(line)
    if log != None:
        res = re.compile('Node id: (\d+), Rime address: ([a-f\d:]+)').match(log)
        if res != None:
            currId = int(res.group(1))
            address = res.group(2)
            idMacMap[id] = {'currId': currId, 'address': address}
        
for id in idMacMap:
    if id != idMacMap[id]['currId']:
        print 'Warning: node %d has current nodeid %d!' %(id, idMacMap[id]['currId'])
        
print "Number of nodes active: %u" %(len(idMacMap))
        
for id in sorted(idMacMap):
    print "{%3d, {{0x%s}}},"%(id, idMacMap[id]['address'].replace(':', ',0x'))
