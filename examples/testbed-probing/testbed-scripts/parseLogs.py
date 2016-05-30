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
ETXALPHA = 0.15
INITETX = 0.5

def updateMetric(network, sender, receiver, success, numtx, gradientFunc, gradientArg):
    newMetric = 1. if success else 0.
    if network[sender][receiver]["rxCount"] == 0:
        prr = INITETX if success else 0
    else:
        prr = ETXALPHA * newMetric + (1-ETXALPHA) * network[sender][receiver]["prr"]
    network[sender][receiver]["rxCount"] += 1
    network[sender][receiver]["prr"] = prr
    
    if prr > 0 and network[receiver]["gradient"] != None: # there is a link sender -> receiver
        gradient = gradientFunc(network[receiver], prr, numtx, gradientArg)            
        if network[sender]["gradient"] == None or gradient < network[sender]["gradient"]:
            network[sender]["gradient"] = gradient
            network[sender]["e2epdr"] = network[receiver]["e2epdr"] * 1-((1-prr)**numtx)
            network[sender]["hops"] = network[receiver]["hops"] + 1
            network[sender]["prr"] = prr
            network[sender]["parent"] = receiver
    
def updateNode(network, curr, numtx, gradientFunc, gradientArg):
    if curr == ROOT:
        return
    network[curr]["gradient"] = None # start over
    for parent in network:
        if network[curr][parent]["prr"] > 0 and network[parent]["gradient"] != None: # there is a link ID -> curr
            prr = network[curr][parent]["prr"]
            gradient = gradientFunc(network[parent], prr, numtx, gradientArg)            
            if network[curr]["gradient"] == None or gradient < network[curr]["gradient"]:
                network[curr]["gradient"] = gradient
                network[curr]["e2epdr"] = network[parent]["e2epdr"] * 1-((1-prr)**numtx)
                network[curr]["hops"] = network[parent]["hops"] + 1
                network[curr]["prr"] = prr
                network[curr]["parent"] = parent
    
def gradientEtx(parent, prr, numtx, exponent):
    return parent["gradient"] + ((1.0 / prr**exponent))

def gradientE2epdr(parent, prr, numtx, noarg):
    return 1 / (parent["e2epdr"] * 1-((1-prr)**numtx))


def dijkstraStep(network, curr, numtx, gradientFunc, gradientArg):
    for id in network:
        if network[id][curr]["prr"] > 0:
            # there is a link ID -> curr
            prr = network[id][curr]["prr"]
            gradient = gradientFunc(network[curr], prr, numtx, gradientArg)
            
            if network[id]["gradient"] == None or gradient < network[id]["gradient"]:
                network[id]["gradient"] = gradient
                network[id]["e2epdr"] = network[curr]["e2epdr"] * 1-((1-prr)**numtx)
                network[id]["hops"] = network[curr]["hops"] + 1
                network[id]["prr"] = prr
                network[id]["parent"] = curr
            
    network[curr]["visited"] = True
    return

def diskjtra(network, numtx, gradientFunc, gradientArg):
    parentSwitchCount = 0
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
            dijkstraStep(network, currentNode, numtx, gradientFunc, gradientArg)
            nodeCount += 1
        else:
            # no more nodes to visit
            print "Dijkstra done, %u nodes" %(nodeCount)
            parentSwitchCount = 0
            for id in network:
                if network[id]["previousParent"] != network[id]["parent"]:
                    network[id]["previousParent"] = network[id]["parent"]
                    parentSwitchCount += 1
            return network, parentSwitchCount
        
def parseTsch(line, time, id, log, asnInfo, timeline):
    
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

            if not id in allNodes:
                allNodes.append(id)
            
            # process each module separately
            if not module in parsingFunctions:
                continue
                        
            parsingFunction = parsingFunctions[module]                
            if parsingFunction != None:
                parsingFunction(line, time, id, log, asnInfo, timeline)
                      
    print "Parsed %d/%d lines" %(linesParsedCount, len(allLines))
    return timeline, allNodes
          
def runDisktra(timeline, allNodes, numtx, gradientFunc, gradientArg):
    
    print "Number of nodes: %u" %(len(allNodes))
    
    network = {}
    # init
    for sender in allNodes:
        network[sender] = {}
        network[sender]["gradient"] = None        
        network[sender]["previousParent"] = None
        network[sender]["parent"] = None
        for id in allNodes:
            network[sender][id] = {"rxCount": 0, "prr": 0}

    network[ROOT]["gradient"] = 1.0 # RPL ETX 1
    network[ROOT]["e2epdr"] = 1.0
    network[ROOT]["hops"] = 0
    network[ROOT]["prr"] = 1    
    parentSwitchCount = 0
    
    for asn in timeline:
        
        sender = None
        for id in timeline[asn]:
            if timeline[asn][id]['event'] == 'Tx':
                if sender != None:
                    print "Warning!! two senders at asn 0x%x: %u and %u" %(asn, sender, id)
                sender = id
#                break
        
        if sender == None:
            continue
        for id in allNodes:
            prevParent = network[sender]["parent"]
            if id in timeline[asn] and timeline[asn][id]['event'] == 'Rx':
                updateMetric(network, sender, id, True, numtx, gradientFunc, gradientArg)
            else:
                updateMetric(network, sender, id, False, numtx, gradientFunc, gradientArg)
            
            #updateNode(network, sender, numtx, gradientFunc, gradientArg)
            newParent = network[sender]["parent"]
            if newParent != prevParent:
                parentSwitchCount += 1

    #network, disktraParentSwitchCount = diskjtra(network, numtx, gradientFunc, gradientArg)
    #return {"network": network, "parentSwitchCount": parentSwitchCount}
    return {"network": network, "hopsAll": map(lambda x: network[x]["hops"], network), "parentSwitchCount": parentSwitchCount,
            "e2epdrAll": map(lambda x: network[x]["e2epdr"], network), "prrAll": map(lambda x: network[x]["prr"], network)}
    
def main():
    if len(sys.argv) < 2:
        dir = '.'
    else:
        dir = sys.argv[1].rstrip('/')
    file = os.path.join(dir, "log.txt")
    
    timeline, allNodes = parse(file)
    res = runDisktra(timeline, allNodes, 9, gradientEtx, 1)
    
    #hopsAll = 
    #e2epdrAll = map(lambda x: network[x]["e2epdr"], network)
    #prrAll = map(lambda x: network[x]["prr"], network)
    
    print "parent switches %d"%(res["parentSwitchCount"])
    print "hops: mean %f" %(np.mean(res["hopsAll"]))
    print "prr: mean %f" %(np.mean(res["prrAll"]))
    print "e22pdr: mean %f" %(np.mean(res["e2epdrAll"]))
    print "e22pdr: median %f" %(np.median(res["e2epdrAll"]))
    print "e22pdr: 90p %f" %(np.percentile(res["e2epdrAll"], 10))
    print "e22pdr: min %f" %(np.min(res["e2epdrAll"]))
