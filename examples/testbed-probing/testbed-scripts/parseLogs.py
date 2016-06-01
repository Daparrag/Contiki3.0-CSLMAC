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
    
def rtxSuccessRate(prr, numtx):
    #return 1-((1-prr)**numtx)
    return min(1-((1-prr)**numtx), 1-(10**-16))
    
def guess_etx_from_rssi(rssi):
    ETX_INIT_MAX = 3
    RSSI_HIGH = -60
    RSSI_LOW = -90
    RSSI_DIFF = (RSSI_HIGH - RSSI_LOW)
    bounded_rssi = rssi
    bounded_rssi = min(bounded_rssi, RSSI_HIGH)
    bounded_rssi = max(bounded_rssi, RSSI_LOW + 1)
    etx = RSSI_DIFF / (bounded_rssi - RSSI_LOW)
    return min(etx, ETX_INIT_MAX)
    
def updateMetric(network, sender, receiver, success, numtx, gradientFunc, gradientArg, newRssi):
    gradient = None
    newMetric = 1. if success else 0.
    if network[sender][receiver]["updateCount"] == 0:
        prr = INITETX if success else 0.
        rssi = newRssi
    else:
        prr = ETXALPHA * newMetric + (1-ETXALPHA) * network[sender][receiver]["prr"]
        rssi = ETXALPHA * newRssi + (1-ETXALPHA) * network[sender][receiver]["rssi"]
    if success:
        network[sender][receiver]["rxCount"] += 1
    network[sender][receiver]["updateCount"] += 1
    network[sender][receiver]["prr"] = prr
    network[sender][receiver]["rssi"] = rssi
            
    if prr > 0 and network[receiver]["gradient"] != None: # there is a link sender -> receiver
        if network[receiver][sender]["rxCount"] > 0: # we have heard at least once from the node
            gradient = gradientFunc(network, sender, receiver, prr, numtx, gradientArg)            
            if network[sender]["gradient"] == None or gradient < network[sender]["gradient"]:
                network[sender]["parent"] = receiver  
    
                
    if network[sender]["parent"] == receiver:
        downprr = network[receiver][sender]["prr"]
        if gradient == None:
            gradient = gradientFunc(network, sender, receiver, prr, numtx, gradientArg)
        network[sender]["e2epdr"] = network[receiver]["e2epdr"] * rtxSuccessRate(prr, numtx)
        network[sender]["gradient"] = gradient
        network[sender]["hops"] = network[receiver]["hops"] + 1
        network[sender]["prr"] = prr
        network[sender]["parent"] = receiver
        
        network[sender]["downprr"] = downprr
        network[sender]["downe2epdr"] = network[receiver]["downe2epdr"] * rtxSuccessRate(downprr, numtx)

def gradientEtxRssi(network, child, parent, prr, numtx, exponent):
    return network[parent]["gradient"] + ((((1.0 / prr**exponent)) + guess_etx_from_rssi(network[child][parent]["rssi"])) / 2)

def gradientEtx(network, child, parent, prr, numtx, exponent):
    return network[parent]["gradient"] + ((1.0 / prr**exponent))

def gradientE2epdr(network, child, parent, prr, numtx, noarg):
    return 1.-(network[parent]["e2epdr"] * rtxSuccessRate(prr, numtx))
        
def parseTsch(line, time, id, log, asnInfo, timeline):
    
    if asnInfo != None:
        asn = asnInfo['asn'] 
        slotframe = asnInfo['slotframe']
        timeslot = asnInfo['timeslot']
        channel_offset = asnInfo['channel_offset']
        channel = asnInfo['channel']
                
#---- TSCH link: Rx -------------------------------------------------------------------------------------------------------------
        res = re.compile('([ub]c)-([01])-\d+ (\d+) rx (\d+).*rssi ([-\d]+)').match(log)
        
        if res:
            is_unicast = res.group(1) == "uc"
            is_data = int(res.group(2))
            datalen = int(res.group(3))
            src = int(res.group(4))
            rssi = int(res.group(5))
            
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
    
    #for line in open(file, 'r').readlines():
    lineCount = 0
    for line in open(file, 'r').readlines()[:500000]:
    #for line in open(file, 'r').readlines()[-20000:]:

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
                ret = parsingFunction(line, time, id, log, asnInfo, timeline)
                      
    print "Parsed %d/%d lines" %(linesParsedCount, lineCount)
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
        network[sender]["parentSwitchCount"] = 0
        network[sender]["hopSum"] = 0.
        network[sender]["hopSumCount"] = 0
        for id in allNodes:
            network[sender][id] = {"rxCount": 0, "updateCount": 0, "prr": 0}

    network[ROOT]["gradient"] = 0.0 # ETX 0 or loss rate 0
    network[ROOT]["e2epdr"] = 1.0
    network[ROOT]["downe2epdr"] = 1.0
    network[ROOT]["hops"] = 0.0
    network[ROOT]["prr"] = 1.0   
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
                updateMetric(network, sender, id, True, numtx, gradientFunc, gradientArg, timeline[asn][id]['rssi'])
            else:
                updateMetric(network, sender, id, False, numtx, gradientFunc, gradientArg, 0)
            
            newParent = network[sender]["parent"]
            if newParent != prevParent:
                network[sender]["parentSwitchCount"] += 1
            if network[sender]["gradient"] != None: # the node is connected
                network[sender]["hopSum"] += network[sender]["hops"]
                network[sender]["hopSumCount"] += 1
    
    for id in allNodes:
        parent = network[id]["parent"]
        network[id]["hops"] = network[id]["hopSum"] / network[id]["hopSumCount"]
       # if parent != None:
        #    prr = network[id][parent]["prr"]
         #   network[id]["downprr"] = network[parent][id]["prr"]
        
    del network[ROOT]

    return {"network": network, "hopsAll": map(lambda x: network[x]["hops"], network), "switchAll": map(lambda x: network[x]["parentSwitchCount"], network),
            "e2epdrAll": map(lambda x: network[x]["e2epdr"], network), "prrAll": map(lambda x: network[x]["prr"], network),
            "downPrrAll": map(lambda x: network[x]["downprr"], network),
            "prrAsym": map(lambda x: network[x]["downprr"] - network[x]["prr"], network),
            "downe2epdrAll": map(lambda x: network[x]["downe2epdr"], network)
            }

def getAllPrr(res):
    network = res["network"]
    allPrr = []
    for i in network.keys():
        for j in network.keys():
            if j > i:
                if network[i][j]["prr"] > 0 or network[j][i]["prr"] > 0:
                    allPrr.append((100*network[i][j]["prr"], 100*network[j][i]["prr"]))
    allPrr = allPrr[::10]
    allPrr = zip(*sorted(allPrr, reverse=True))
    return allPrr

def getPlotData(timeline, allNodes):
    resEtx = runDisktra(timeline, allNodes, 9, gradientEtx, 1)
    resEtx2 = runDisktra(timeline, allNodes, 9, gradientEtx, 2)
    resEtx3 = runDisktra(timeline, allNodes, 9, gradientEtx, 3)
    resEtx4 = runDisktra(timeline, allNodes, 9, gradientEtx, 4)
    resE2e = runDisktra(timeline, allNodes, 9, gradientE2epdr, 0)
    #resEtx2Rssi = runDisktra(timeline, allNodes, 9, gradientEtxRssi, 2)
    return [resEtx, resEtx2, resEtx3, resEtx4, resE2e]

def doPlot(allRes, filename, ylabel, ymin=None, ymax=None, yscale=None):
    matplotlib.rcParams.update({'font.size': 22})
    plt.figure()
    plt.boxplot(allRes)
    plt.xlabel("Metric", fontsize = 28)
    plt.xticks(range(1,6), ["ETX", "ETX$^2$", "ETX$^3$", "ETX$^4$", "1-PDR"])
    plt.ylabel(ylabel, fontsize = 28)
    if yscale != None:
        plt.yscale(yscale)
    plt.ylim(ymin, ymax)
    plt.grid()
    plt.savefig('plot_%s.pdf'%(filename), format='pdf', bbox_inches='tight', pad_inches=0.05)

def getColor(prr):
    c1 = (0,   1, 0.5)
    c2 = (0.5, 0, 1)
    return ((c1[0]*prr + c2[0]*(1-prr)), (c1[1]*prr + c2[1]*(1-prr)), (c1[2]*prr + c2[2]*(1-prr)))

def plotAsym(ax, data, filename, ylabel=False):
    matplotlib.rcParams.update({'font.size': 16})
    #plt.figure(figsize=(3,8))
    
    for i in range(len(data[0])):
        ax.plot([data[0][i], data[1][i]], c = getColor(data[0][i] / 100.), alpha=0.5)
    #ax.ylim(0, 4)
    #ax.ylim(0, 100)
    #if ylabel:
     #   plt.ylabel("Link PRR (%)", fontsize = 20)
    
def plotAll(allRes):
    
    doPlot(list(map(lambda x: x["hopsAll"], allRes)), "hops", "Hops (#)", ymin=0)
    doPlot(list(array(map(lambda x: x["prrAll"], allRes)) * 100) , "prr", "Link PRR (%)", ymin=0, ymax=101)
    doPlot(list(array(map(lambda x: x["downPrrAll"], allRes)) * 100) , "downprr", "Down Link PRR (%)", ymin=0, ymax=101)
    doPlot(list(1 - array(map(lambda x: x["e2epdrAll"], allRes))), "pdr", "E2E Loss Rate", yscale='log', ymax=1, ymin=10**-16) #ymax=1, ymin=10**-16
    doPlot(list(1 - array(map(lambda x: x["downe2epdrAll"], allRes))), "downpdr", "Down E2E Loss Rate", yscale='log', ymax=1, ymin=10**-16)
    doPlot(list(map(lambda x: x["switchAll"], allRes)), "ps", "Parent Switches (#)")
    doPlot(list(array(map(lambda x: x["prrAsym"], allRes)) * 100) , "prrasym", "Link Asymmetry (pp)")
    
    resEtx = allRes[0]
    resEtx2 = allRes[1]
    resE2e = allRes[4]
    
    f, (ax1, ax2, ax3) = plt.subplots(1, 3, sharey=True)
    ax1.set_ylabel('Link PRR (%)')
    ax1.set_title("All links")
    ax1.set_xticks([])
    ax2.set_title("ETX")
    ax2.set_xticks([0, 1])
    ax2.set_xticklabels(["Up", "Down"])
    ax2.get_xaxis().majorTicks[1].label1.set_horizontalalignment('right')
    ax2.get_xaxis().majorTicks[0].label1.set_horizontalalignment('left')
    ax3.set_title("ETX$^2$")
    ax3.set_xticks([0, 1])
    ax3.set_xticklabels(["Up", "Down"])
    ax3.get_xaxis().majorTicks[1].label1.set_horizontalalignment('right')
    ax3.get_xaxis().majorTicks[0].label1.set_horizontalalignment('left')    
    #plt.setp((ax1, ax2, ax3), xticks = range(2), xtickslabels=["Up", "Down"])
    plotAsym(ax1, getAllPrr(resEtx), "alllinks")
    plotAsym(ax2, [100*array(resEtx["prrAll"]), 100*array(resEtx["downPrrAll"])], "etx", ylabel=True)
    plotAsym(ax3, [100*array(resEtx2["prrAll"]), 100*array(resEtx2["downPrrAll"])], "etx2")
    #plotAsym([100*array(resE2e["prrAll"]), 100*array(resE2e["downPrrAll"])], "e2e")
    f.savefig('plot_parallel_prr_all.pdf', format='pdf', bbox_inches='tight', pad_inches=0.05)

def main():
    if len(sys.argv) < 2:
        dir = '.'
    else:
        dir = sys.argv[1].rstrip('/')
    file = os.path.join(dir, "log.txt")
    
    timeline, allNodes = parse(file)
    allRes = getPlotData(timeline, allNodes)
    plotAll(allRes)
    
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
    
#execfile("../parseLogs.py")
#timeline, allNodes = parse("log.txt")
#allRes = getPlotData(timeline, allNodes)
#plotAll(allRes)
