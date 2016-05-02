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
network = {}
timeline = OrderedDict()
parentSwitchCount = 0

def updateMetric(link, success):
    newMetric = 1. if success else 0.
    if link["rxCount"] == 0:
        link["prr"] = newMetric
    else:
        link["prr"] = 0.15 * newMetric + 0.85 * link["prr"]
    link["rxCount"] += 1
    
def calculateGradient(parent, child):
    prr = network[child][parent]["prr"]
    #return network[parent]["gradient"] + ((1.0 / prr**2))
    return 1 / (network[parent]["e2epdr"] * 1-((1-prr)**16))

def dijkstraStep(curr):
    for id in network:
        if network[id][curr]["prr"] > 0:
            # there is a link ID -> curr
            prr = network[id][curr]["prr"]
            gradient = calculateGradient(curr, id)
            if network[id]["gradient"] == None or gradient < network[id]["gradient"]:
                network[id]["gradient"] = gradient
                network[id]["e2epdr"] = network[curr]["e2epdr"] * 1-((1-prr)**16)
                network[id]["hops"] = network[curr]["hops"] + 1
                network[id]["prr"] = prr
                network[id]["parent"] = curr
            
    network[curr]["visited"] = True
    return

def diskjtra():
    global network, parentSwitchCount
    # init
    for id in network:
        network[id]["parent"] = None
        network[id]["gradient"] = None
        network[id]["visited"] = False
    
    network[ROOT]["gradient"] = 1.0 # RPL ETX 1
    network[ROOT]["e2epdr"] = 1.0
    network[ROOT]["hops"] = 0
    network[ROOT]["prr"] = 1
    nodeCount = 0
    
    while True:
        # select current node and run it
        minGradient = None
        currentNode = None
        for id in network:
            if not network[id]["visited"] and network[id]["gradient"] != None:
                if minGradient == None or network[id]["gradient"] < minGradient:
                    minGradient = network[id]["gradient"]
                    currentNode = id
        if currentNode != None:
            dijkstraStep(currentNode)
            nodeCount += 1
        else:
            # no more nodes to visit
            print "Dijkstra done, %u nodes" %(nodeCount)
            parentSwitchCount = 0
            for id in network:
                if network[id]["previousParent"] != network[id]["parent"]:
                    network[id]["previousParent"] = network[id]["parent"]
                    parentSwitchCount += 1
            return
        
def parseTsch(line, time, id, log, asnInfo):
    global appDataStats, receivedList, timeline
    
    if asnInfo != None:
        asn = asnInfo['asn'] 
        slotframe = asnInfo['slotframe']
        timeslot = asnInfo['timeslot']
        channel_offset = asnInfo['channel_offset']
        channel = asnInfo['channel']
                
#---- TSCH link: Rx -------------------------------------------------------------------------------------------------------------
        res = re.compile('([ub]c)-([01])-\d+ (\d+) rx (\d+), edr ([-\d]+)').match(log)

        if res:
            is_unicast = res.group(1) == "uc"
            is_data = int(res.group(2))
            datalen = int(res.group(3))
            src = int(res.group(4))
            edr = int(res.group(5))
            rssi = 0#int(res.group(6))
            
            moduleInfo = {'event': 'Rx', 'src': src, 'asnInfo': asnInfo, 'rssi': rssi }
                                    
            if not asn in timeline:
                timeline[asn] = OrderedDict()
            timeline[asn][id] = moduleInfo
            
            return moduleInfo

#---- TSCH link: Tx -------------------------------------------------------------------------------------------------------------                
        res = re.compile('([ub]c)-([01])-[01] (\d+) tx (\d+), st (\d+)-(\d+)(.*)$').match(log)
        if res:
            is_unicast = res.group(1) == "uc"
            is_data = int(res.group(2))
            datalen = int(res.group(3))
            dest = int(res.group(4))
            status = int(res.group(5))
            txCount = int(res.group(6))
            
            moduleInfo = {'event': 'Tx', 'dest': dest, 'asnInfo': asnInfo }
            
            if not asn in timeline:
                timeline[asn] = OrderedDict()
            timeline[asn][id] = moduleInfo
                                            
            return moduleInfo

    return None

################################################################################################################################
   
def parseLine(line):
    res = re.compile('^(\d+)\.(\d+);m3-(\d+);(.*)$').match(line)
    if res:
        return int(res.group(1)) * 1000000 + int(res.group(2)), int(res.group(3)), res.group(4)
    return None, None, None

################################################################################################################################

def main():
    
    if len(sys.argv) < 2:
        dir = '.'
    else:
        dir = sys.argv[1].rstrip('/')
    file = os.path.join(dir, "log.txt")

    allData = []
    baseTime = None
    lastPrintedTime = 0
    time = None
    nonExtractedModules = []
    parsingFunctions = {
                        'TSCH': parseTsch,
                        }
    
    linesParsedCount = 0
    allLines = open(file, 'r').readlines()
    
    #for line in allLines:
    for line in open(file, 'r').readlines()[:300000]:
    #for line in open(file, 'r').readlines()[-20000:]:

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
            
            # match lines that include asn info
            res = re.compile('^\{asn-([a-f\d]+).([a-f\d]+) link-(\d+)-(\d+)-(\d+)-(\d+) [\s\d-]*ch-(\d+)\} (.*)').match(log)
            if res:
                asn_ms = int(res.group(1), 16)
                asn_ls = int(res.group(2), 16)
                #asn = (asn_ms << 32) + asn_ls
                # ignore most significant byte (takes 2 years of update to overflow) for quicker processing
                asn = asn_ls 
                slotframe = int(res.group(3))
                slotframe_len = int(res.group(4))
                timeslot = int(res.group(5))
                channel_offset = int(res.group(6))
                channel = int(res.group(7))
                log = res.group(8)
                asnInfo = {'asn': asn,
                      'slotframe': slotframe, 'slotframe_len': slotframe_len, 'timeslot': timeslot, 'channel_offset': channel_offset,
                      'channel': channel }

            linesParsedCount += 1

            if not id in network:
                network[id] = {}
            
            # process each module separately
            if not module in parsingFunctions:
                continue
                        
            parsingFunction = parsingFunctions[module]                
            if parsingFunction != None:
                moduleInfo = parsingFunction(line, time, id, log, asnInfo)
                      
    print "Parsed %d/%d lines" %(linesParsedCount, len(allLines))
          
    #asn = 1516
    #for node in  timeline[asn]:
    #   print timeline[asn][node]
    print "Number of nodes: %u" %(len(network.keys()))
    #for id in network.keys():
    
    # init
    for sender in network.keys():
        network[sender]["previousParent"] = None
        for id in network.keys():
            network[sender][id] = {"rxCount": 0, "prr": 0}
    
    round = 0
    for asn in timeline:
        #print "ASN: %x" %(asn)
        sender = None
        for id in timeline[asn]:
            if timeline[asn][id]['event'] == 'Tx':
                sender = id
                break
        
        if sender == None:
            continue
        for id in network.keys():
            if id in timeline[asn] and timeline[asn][id]['event'] == 'Rx':
                updateMetric(network[sender][id], True)
            else:
                updateMetric(network[sender][id], False) 
        
    diskjtra()
    hopsAll = map(lambda x: network[x]["hops"], network)
    e2epdrAll = map(lambda x: network[x]["e2epdr"], network)
    prrAll = map(lambda x: network[x]["prr"], network)
    print "--- Round %u"%(round)
    print "parent switches %d"%(parentSwitchCount)
    print "hops: mean %f" %(np.mean(hopsAll))
    print "prr: mean %f" %(np.mean(prrAll))
    print "e22pdr: mean %f" %(np.mean(e2epdrAll))
    print "e22pdr: median %f" %(np.median(e2epdrAll))
    print "e22pdr: 90p %f" %(np.percentile(e2epdrAll, 10))
    print "e22pdr: min %f" %(np.min(e2epdrAll))

main()
