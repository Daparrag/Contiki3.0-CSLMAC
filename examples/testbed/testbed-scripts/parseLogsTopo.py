#!/usr/bin/env python

import os
import re
import fileinput
import math
from sets import Set
from pylab import *
from collections import OrderedDict
import numpy as np

ROOT = 240

topology = {}
#excludedNodes = {29, 49, 62, 63, 83, 160, 162, 184, 226, 359}
excludedNodes = {}

# differents levels
# 1) uptodate and consistent
# 2) outdated but consistent (reachable from all)
# 3) outdated, stalled routes (still reachable from root)
# 4) outdated, unreachable from root
# 5) not joined

def parseStatDefaultRoute(log):
    res = re.compile('NetStatus: (\d+) \(lifetime').match(log)
    if res:
        parent = int(res.group(1))
        return parent
    return None

def parseStatRoute(log):
    res = re.compile('NetStatus: (\d+) via (\d+) \(lifetime').match(log)
    if res:
        destination = int(res.group(1))
        nexthop = int(res.group(2))
        return destination, nexthop
    return None, None

def parseStatLink(log):
    res = re.compile('NetStatus: (\d+) to (\d+) \(lifetime').match(log)
    if res:
        child = int(res.group(1))
        parent = int(res.group(2))
        return child, parent
    return None, None

def parseStatHeader(log):
    res = re.compile('--- Network status --- \(asn (\d+) (\d+)\)').match(log)
    if res:
        asn = int(res.group(1))
        targetAsn = int(res.group(2))
        return asn, targetAsn
    return None, None

def parseStatLine(log):
    res = re.compile('\[(\d+)\] (.*)').match(log)
    if res:
        count = int(res.group(1))
        msg = res.group(2)
        return count, msg
    return None, None

def parseStat(line, time, id, log):
    count, msg = parseStatLine(log)
    if count != None:
        asn, targetAsn = parseStatHeader(msg)
        if asn != None:     
            if asn < (targetAsn + 100): # ignore delayed printouts (happens at bootstrap)
                if not count in topology:
                    topology[count] = {} 
                if not id in topology[count]:
                    topology[count][id] = {}
                    
        parent = parseStatDefaultRoute(msg)
        if parent != None:
            if count in topology and id in topology[count]:
                topology[count][id]["parent"] = parent
            return topology

        destination, nexthop = parseStatRoute(msg)
        if destination != None:
            if count in topology and id in topology[count]:
                if not "routes" in topology[count][id]:
                    topology[count][id]["routes"] = {}
                topology[count][id]["routes"][destination] = nexthop
            return topology

        child, parent = parseStatLink(msg)
        if child != None:
            if count in topology and id in topology[count]:
                if not "links" in topology[count][id]:
                    topology[count][id]["links"] = {}
                topology[count][id]["links"][child] = parent
            return topology

    return None

################################################################################################################################
   
def parseLine(line):
    res = re.compile('^(\d+)\.(\d+);m3-(\d+);(.*)$').match(line)
    if res:
        return int(res.group(1)) * 1000000 + int(res.group(2)), int(res.group(3)), res.group(4)
    return None, None, None

################################################################################################################################

def parse(file):
    
    network = {}
    allNodes = []
    timeline = OrderedDict()

    allData = []
    baseTime = None
    lastPrintedTime = 0
    time = None
    nonExtractedModules = []
    parsingFunctions = {
                        'Stat': parseStat,
                        }
    
    linesParsedCount = 0
    
    lineCount = 0
    #for line in open(file, 'r').readlines()[:100000]:
    for line in open(file, 'r').readlines():
    
        lineCount += 1
        log = None
        module = None
        # match time, id, module, log; The common format for all log lines
        t, id, log = parseLine(line)
        if log != None:
            res = re.compile('^([^:]+):\\s*(.*)$').match(log)
            if res:
                module = res.group(1)
                log = res.group(2)

        if module == None or log == None:
            if log != None and len(log) > 0 and not module in nonExtractedModules:
                print "Could not extract module:", line
                nonExtractedModules.append(module)
            continue 
        else:           
            # default for all structures
            moduleInfo = None
            asnInfo = None
            packetId = None
         
            time = t   
            # adjust time to baseTime
            if not baseTime:
                baseTime = time
            if time - lastPrintedTime >= 60*1000000:
                print "%u,"%((time-baseTime) / (60*1000000)),
                sys.stdout.flush()
                lastPrintedTime = time
            time -= baseTime
            time /= 1000000. # time from us to s
    
            linesParsedCount += 1
            
            if id in excludedNodes:
                continue

            if not id in allNodes:
                allNodes.append(id)
            
            # process each module separately
            if not module in parsingFunctions:
                continue
                        
            parsingFunction = parsingFunctions[module]                
            if parsingFunction != None:
                ret = parsingFunction(line, time, id, log)
                      
    print "Parsed %d/%d lines" %(linesParsedCount, lineCount)
    return topology, allNodes

def isKnownNS(topo, node):
    return node in topo[ROOT]["links"].keys()
 
def isReachableNS(topo, node):
    hops = 0
    while node != ROOT and node in topo[ROOT]["links"].keys():
        node = topo[ROOT]["links"][node]
        hops += 1
        if hops > 64: # loop!
            return False
    return node == ROOT

def isConsistentNS(topo, node):
    return True # Always consistent assuming reachable

def isUptodateNS(topo, node):
    hops = 0
    while node in topo[ROOT]["links"].keys():
        parentInLinks = topo[ROOT]["links"][node]
        currentParent = topo[node]["parent"]
        
        node = topo[ROOT]["links"][node]
        hops += 1
        if hops > 64: # loop!
            return False
    return True

def processTopologyNS(topology, allNodes):
    for count in topology.keys()[:-1]: # ignore last (incomplete)
        print "Processing NS %u" %(count)
        topo = topology[count]
        for node in topo.keys():
            if node == ROOT:
                topo[node]["status"] = "Uptodate"
            else:
                topo[node]["status"] = "Unknown"
                if isKnownNS(topo, node):
                    topo[node]["status"] = "Known"
                    if isReachableNS(topo, node):
                        topo[node]["status"] = "Reachable"
                        if isConsistentNS(topo, node):
                            topo[node]["status"] = "Consistent"
                            if isUptodateNS(topo, node):
                                topo[node]["status"] = "Uptodate"
            if topo[node]["status"] != "Uptodate":
                print "[%u] Node %u: %s" %(count, node, topo[node]["status"])

def isKnownST(topo, node):
    return node in topo[ROOT]["routes"].keys()

def routeExistsST(topo, src, dst):
    hops = 0
    current = src
    
    # go up
    while current != ROOT and current in topo and "parent" in topo[current]:
        current = topo[current]["parent"]
        hops += 1
        if hops > 64: # loop!
            return False
    
    if current != ROOT:
        return False
        
    # and down
    while current != dst and current in topo and "routes" in topo[current] and dst in topo[current]["routes"].keys():
        current = topo[current]["routes"][dst]
        hops += 1
        if hops > 64: # loop!
            return False
            
    return current == dst

def isReachableST(topo, node):
    return routeExistsST(topo, ROOT, node)

def isConsistentST(topo, node):
    for src in topo.keys():
        if node == ROOT or "parent" in topo[src]:
            if not routeExistsST(topo, src, node):
                return False
    return True

def isUptodateST(topo, node):
    hops = 0
    
    if not "parent" in topo[node]:
        return False
    parent = topo[node]["parent"]
    
    for n in topo.keys():
        if n in topo and "routes" in topo[n] and node in topo[n]["routes"] and topo[n]["routes"][node] == node:
            # has node as next hop
            if topo[node]["parent"] != n:
                return False
    return True

def processTopologyST(topology, allNodes):
    for count in topology.keys()[:-1]: # ignore last (incomplete)
        print "Processing ST %u" %(count)
        topo = topology[count]
        for node in topo.keys():
            if node == ROOT:
                topo[node]["status"] = "Uptodate"
            else:
                topo[node]["status"] = "Unknown"
                if isKnownST(topo, node):
                    topo[node]["status"] = "Known"
                    if isReachableST(topo, node):
                        topo[node]["status"] = "Reachable"
                        if isConsistentST(topo, node):
                            topo[node]["status"] = "Consistent"
                            if isUptodateST(topo, node):
                                topo[node]["status"] = "Uptodate"
            if topo[node]["status"] != "Uptodate":
                print "[%u] Node %u: %s" %(count, node, topo[node]["status"])

def main():
    if len(sys.argv) < 2:
        dir = '.'
    else:
        dir = sys.argv[1].rstrip('/')
    file = os.path.join(dir, "log.txt")
    
    ret = parse(file)
        
#execfile("../../parseLogsTopo.py")
#topo, allNodes = parse("log.txt")
#processTopologyNS(topo, allNodes)
#processTopologyST(topo, allNodes)