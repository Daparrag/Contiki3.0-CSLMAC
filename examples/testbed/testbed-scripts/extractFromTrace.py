#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
from collections import OrderedDict

#MIN_INTERVAL = 0.1
MIN_INTERVAL = 1

MIN_TIME = 0 # start counting only after 10 minutes (except for timeline)
MAX_TIME = 999999
EDC_DIVISOR = 128
#SINK_ID = 2
SINK_ID = 240
#SINK_ID = 272
plotIndex = 0
TARGET_MIN_TIME = 5

LINK_DURATION = 0.015 # 15 ms (in s)
TICK_DURATION = 0.000030517578125 # 30 us (in s) for Sky
#TICK_DURATION = 1./16000000 # running at 16 MHz for JN5168
TICK_PER_LINK = LINK_DURATION/TICK_DURATION

#excludedNodes = [20, 21, 48, 109]
#excludedNodes = [21, 48, 109]
excludedNodes = []
scriptlog = None

def showStats(stats, unit):
    if stats != None:
        if unit == "%":
            return "  %8.4f %4s +/- %10.2f" %(stats['avg'], unit, stats['stdev'])
        else:
            return "%8.2f   %4s +/- %10.2f" %(stats['avg'], unit, stats['stdev'])
    else:
        return ""

def showStatsMinMax(stats):
    if stats != None:
        return "[%8.2f - %8.2f]" %(stats['min'], stats['max'])
    else:
        return ""

def percentile(data, percentile): 

    index = percentile*(len(data)-1)/100.

    if index == int(index):
        return data[int(index)]
    else:
        f1 = index-int(index)
        f2 = 1 - f1
        v1 = data[int(index)]
        v2 = data[int(index)+1]
        return (v1+v2)/2.

def average(data): return sum(data) / float(len(data))

def median(data):
    if len(data) % 2 == 1:
        return data[(len(data)-1)/2]
    else:
        v1 = data[(len(data)/2) - 1]
        v2 = data[len(data)/2]
        return (v1+v2)/2.

def allStats(data):
    if data == None or data == []:
        return {
            'min': 0,
            'max': 0, 
            'avg': 0,
            'stdev': 0,
            'p50': 0,
            'p80': 0,
            'p90': 0,
            'p95': 0,
            'p98': 0
            }
    data.sort()
    avg = average(data)
    datasum = sum(data)
#    med = median(data)
    stdev = math.sqrt(average(map(lambda x: (x - avg)**2, data)))
    return {
            'min': min(data),
            'max': max(data), 
            'avg': avg,
#            'med': med,
            'stdev': stdev,
            'sum': datasum,
            'p50': percentile(data,50),
            'p80': percentile(data,80),
            'p90': percentile(data,90),
            'p95': percentile(data,95),
            'p98': percentile(data,98)
            }

def generateDataFiles(parsed, name, unit, globalData, nodeIDs, perNodeIndexData, timelineData, perNodeGlobal, doSum, doMax):
    global plotIndex
    destDir = os.path.join(parsed['dir'], "data")
    if not os.path.exists(destDir):
        os.makedirs(destDir)
    
    #if globalData and (not(globalData['min'] == 0 and globalData['max'] == 0) or name=='PRR'):
    if True:
        plotIndex += 1
        baseName = os.path.join(destDir, "%02u_%s"%(plotIndex, name))
        
        filePerNode = open(baseName + '_pernode.txt', 'w')
        filePerNode.write("# Metric: '%s', unit '%s', doSum %d, doMax %d\n" %(name, unit, 1 if doSum else 0, 1 if doMax else 0))
        filePerNode.write("# Global: %f %f %f %f %f %f %f %s\n" %(globalData['avg'], globalData['min'], globalData['max'], globalData['stdev'], perNodeGlobal['stdev'] if perNodeGlobal != None else 0, perNodeGlobal['min'] if perNodeGlobal != None else 0, perNodeGlobal['max'] if perNodeGlobal != None else 0, unit))
        filePerNode.write("# Percentiles: 50=%f 80=%f 90=%f 95=%f 98=%f\n" %(globalData['p50'], globalData['p80'], globalData['p90'], globalData['p95'], globalData['p98']))
        filePerNode.write("# PerNode Summary: %f %f %f %f\n" %(perNodeGlobal['avg'], perNodeGlobal['min'], perNodeGlobal['max'], perNodeGlobal['stdev']))
        for node in perNodeIndexData:
            filePerNode.write("%d %d %f %f %f %f\n" %(node, nodeIDs[node], perNodeIndexData[node]['avg'], perNodeIndexData[node]['min'], perNodeIndexData[node]['max'], perNodeIndexData[node]['stdev']))    
        
        fileTimeline = open(baseName + '_timeline.txt', 'w')
        fileTimeline.write("# Global: %f %f %f %f %f %f %f %s\n" %(globalData['avg'], globalData['min'], globalData['max'], globalData['stdev'], perNodeGlobal['stdev'] if perNodeGlobal != None else 0, perNodeGlobal['min'] if perNodeGlobal != None else 0, perNodeGlobal['max'] if perNodeGlobal != None else 0, unit))
        tlist = timelineData.keys()
        tlist.sort()
        for t in tlist:
            fileTimeline.write("%f %f %f %f %f\n" %(t, timelineData[t]['avg'], timelineData[t]['min'], timelineData[t]['max'], timelineData[t]['stdev']))

def extractData(parsed, name, unit, condition, extractField, expectedRange, period, doSum=False, revert=False, doMax=False, verbose=False, export=True, debug=False, weightFunction=None):
    global plotIndex
    global scriptlog
    dataset = parsed['dataset']
    data = []
    intervals = []
    outOfRange = 0
    validCount = 0
        
    str = "__ Extracting %s (%s)" %(name, unit)
    print str,
    sys.stdout.flush()
    scriptlog.write(str + "\n")
        
    for line in dataset:
        if condition(line):
            value = extractField(line)
            id = line['id']
            if revert and id == SINK_ID:
                id = line['packet']['dst']
            
            time = line['time']
            #if 'latency' in line['info'] and line['info']['latency']>0:
             #   time += line['info']['latency']
            time /= 60.
            
            withinTimeBounds = time >= MIN_TIME and time <= MAX_TIME
                
            interval = int(time / period) * period
            if value < expectedRange['min'] or value > expectedRange['max']:
                outOfRange += 1
                if verbose:
                    t_us = time * 60 * 1000 * 1000
            
                    t_ms = t_us / 1000.
                    t_s = t_ms / 1000.
                    t_m = t_s / 60.
                
                    time_str = "%3u:%02u.%03u" %(t_m, t_s%60, t_ms%1000)
                    
                    if outOfRange == 1:
                        str = ""
                        print str
                    
                    if 'lastLine' in line['info']:
                        str = '____%c %s ID:%3d Packet %06x %s: %.2f | %s' %('*' if withinTimeBounds else ' ', time_str, id, line['packet']['id'] if line['packet'] != None else 0, name, value, line['info']['lastLine'])
                        print str,
                        scriptlog.write(str)
                    else:
                        str = '____%c %s ID:%3d Packet %06x %s: %.2f' %('*' if withinTimeBounds else ' ', time_str, id, line['packet']['id'] if line['packet'] != None else 0, name, value)
                        print str
                        scriptlog.write(str + "\n")
            else:
                if withinTimeBounds:
                    validCount += 1

            if weightFunction != None:
                weight = weightFunction(line)
            else:
                weight = 1
                
            for i in range(weight):
                data.append({'value': value, 'id': id, 'interval': interval, 'time': time})
            if not interval in intervals:
                intervals.append(interval)
                       
    nodeIDs = filter(lambda x: x not in excludedNodes, parsed['nodeIDs'])
    index = 0
    perNodeIndexData = {}
    for id in nodeIDs:
#        perNodeDataMinTime = 0
        perNodeDataMinTime = MIN_TIME
        nodeDataList = map(lambda x: x['value'], filter(lambda x: x['id'] == id and x['time'] >= perNodeDataMinTime, data))
        
        if nodeDataList == []:
            perNodeIndexData[index] = {'id': id, 'avg': 0, 'min': 0, 'max':0, 'stdev': 0}
        else:
            if doSum:
                nodeAllStats = allStats([sum(nodeDataList)])
            elif doMax:
                nodeAllStats = allStats([max(nodeDataList)])
            else:
                nodeAllStats = allStats(nodeDataList)
            perNodeIndexData[index] = dict(nodeAllStats.items() + {'id': id}.items())
    
        index += 1

    #keys = filter(lambda x: perNodeIndexData[x]['min'] != 0 or perNodeIndexData[x]['max'] != 0, perNodeIndexData.keys()[1::])#exclude root and nodes with no stats from stdev and percentiles
    keys = perNodeIndexData.keys()[1::]
    perNodeGlobal = allStats(map(lambda x: perNodeIndexData[x]['avg'], keys)) 
    
    timelineData = {}
    for interval in intervals:
        intervalDataList = map(lambda x: x['value'], filter(lambda x: x['interval'] == interval, data))
        if doSum:
            intervalAllStats = allStats([sum(intervalDataList)])
        else:
            intervalAllStats = allStats(intervalDataList)
        timelineData[interval] = intervalAllStats
    
    globalDataList = map(lambda x: x['value'], filter(lambda x: x['time'] >= MIN_TIME and x['time'] <= MAX_TIME, data))
    if doSum:
        globalData = allStats(globalDataList)
        globalData['avg'] = 0
    else:
        globalData = allStats(globalDataList)
                      
    if export:
        generateDataFiles(parsed, name, unit, globalData, nodeIDs, perNodeIndexData, timelineData, perNodeGlobal, doSum, doMax)
                                
    if doSum and 'sum' in perNodeGlobal:
        print " => Sum %8.2f " %(perNodeGlobal['sum']),
    elif doMax:
        print " => Max %8.2f " %(perNodeGlobal['max']),
    else:
        print " => Avg %8.2f " %(globalData['avg']),
        
    if(outOfRange > 0):
        print " Warning: %u %s values out of range!" %(outOfRange, name),
    print ""
                                
    return {'name': name, 'unit': unit, 'doMax': doMax, 'doSum': doSum,
            'global': globalData,
            'perNodeGlobal': perNodeGlobal, 'perNode': perNodeIndexData,
            'timeline': timelineData,
            'count': len(globalDataList), 'validCount': validCount}

def generateTimelineFileVector(dir, parsed, txOnly=False):
        
    nodeIDs = sorted(parsed['nodeIDs'])
    timeline = parsed['timeline']
    
    id_to_index = {}
    for node in nodeIDs:
        id_to_index[node] = nodeIDs.index(node)
        
    firstAsn = timeline.keys()[0]
    lastAsn = timeline.keys()[-1]
    
    data_x = {}
    data_y = {}
    
    for asn in timeline.keys():
        asnEvents = timeline[asn]
        for node in asnEvents:
            event = asnEvents[node]['event']
            asnInfo = asnEvents[node]['asnInfo']
            slotframe = asnInfo['slotframe']
            if not (slotframe, event) in data_x:
                data_x[(slotframe, event)] = []
                data_y[(slotframe, event)] = []
            data_x[(slotframe, event)].append(asn-firstAsn)
            data_y[(slotframe, event)].append(id_to_index[node])
    for k in data_x.keys():
        data_x[k] = array(data_x[k])
        data_y[k] = array(data_y[k])
        data_x[k] = data_x[k] * 0.015
    
    fig = plt.figure(figsize=(20, 3))
    ax = fig.add_subplot(111)
    #ax.axis(ymin=0,ymax=len(nodeIDs),xmin=0,xmax=(lastAsn-firstAsn)*0.015)
    ax.axis(ymin=0,ymax=len(nodeIDs),xmin=0,xmax=9)
         
    colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']
    ax.scatter(data_x[(0,"Tx")], data_y[(0,"Tx")], marker='s', edgecolor='none', color=colors[0],s=5)
    ax.scatter(data_x[(0,"Rx")], data_y[(0,"Rx")], marker='o', edgecolor='none', color=colors[0],s=2)
    ax.scatter(data_x[(1,"Tx")], data_y[(1,"Tx")], marker='s', edgecolor='none', color=colors[1],s=5)
    ax.scatter(data_x[(1,"Rx")], data_y[(1,"Rx")], marker='o', edgecolor='none', color=colors[1],s=2)
    ax.scatter(data_x[(2,"Tx")], data_y[(2,"Tx")], marker='s', edgecolor='none', color=colors[2],s=5)
    ax.scatter(data_x[(2,"Rx")], data_y[(2,"Rx")], marker='o', edgecolor='none', color=colors[2],s=2)
    plt.gca().invert_yaxis()
    ax.set_xlabel("Time (s)", fontsize=16)
    ax.set_ylabel("Node Index", fontsize=16)
    #plt.axis('off')
    fig.savefig('timeline.pdf', format='pdf', bbox_inches='tight', pad_inches=0)

def generateTimelineFile(dir, parsed, txOnly=False):
        
    nodeIDs = parsed['nodeIDs']
    timeline = parsed['timeline']
    
    if len(timeline) == 0:
        return
    
    if txOnly:
        trafficFile = open(os.path.join(dir, "timeline-txonly.txt"), 'w')
    else:
        trafficFile = open(os.path.join(dir, "timeline.txt"), 'w')
    trafficFile.write("asn   \ nodeID |")
    firstAsn = timeline.keys()[0]
    lastAsn = timeline.keys()[-1]
    for node in nodeIDs:
        trafficFile.write("%2x|" %(node))
    trafficFile.write("\n")
    #for asn in timeline.keys():
    for asn in range(firstAsn, lastAsn, 1):
        if asn in timeline:
            asnEvents = timeline[asn]
            trafficFile.write("asn-%010x |" %(asn))
            for node in nodeIDs:
                if node in asnEvents:
                    event = asnEvents[node]['event']
                    if event == 'Tx':
                        if asnEvents[node]['is_unicast']:
                            trafficFile.write("T%c|"%("+" if asnEvents[node]['status'] == 0 else "-"))
                        else:
                            trafficFile.write("TB|")
                    else:
                        if txOnly:
                            trafficFile.write("  |")
                        else:
                            trafficFile.write(" R|")
                else:
                    trafficFile.write("  |")
            trafficFile.write("\n")
            if not txOnly:
                trafficFile.write("               |")
                for node in nodeIDs:
                    if node in asnEvents:
                        event = asnEvents[node]['event']
                        if event == 'Tx':
                            trafficFile.write("%2x|"%(asnEvents[node]['nextHop']))
                        else:
                            trafficFile.write("%2x|" %asnEvents[node]['prevHop'])
                    else:
                        trafficFile.write("  |")
                trafficFile.write("\n")
        else:
            trafficFile.write("asn-%010x |" %(asn))
            for node in nodeIDs:
                trafficFile.write("  |")
            trafficFile.write("\n")
            if not txOnly:
                trafficFile.write("               |")
                for node in nodeIDs:
                    trafficFile.write("  |")
                trafficFile.write("\n")
            
def connected(txGraph, s, r, channel):
    return (s in txGraph and r in txGraph[s]
                                and channel in txGraph[s][r]
                                and txGraph[s][r][channel] > 0)
            
def extractProbing(dir, parsed):
    probingFile = open(os.path.join(dir, "probing.txt"), 'w')
    dataset = parsed['dataset']
    links = {}
    maxRxCount = 0
    totalTxCount = 0
    totalRxCount = 0
    for line in dataset:
        if line['module'] == "App":
            if line['info']['event'] == "sending":
                totalTxCount += 1
                continue
            elif line['info']['event'] == "received":
                totalRxCount += 1
                dst = line['id']
                src = line['packet']['src']
                if not (src,dst) in links:
                    links[(src,dst)] = {'rxCount': 0}
                links[(src,dst)]['rxCount'] += 1
                maxRxCount = max(maxRxCount, links[(src,dst)]['rxCount'])
    cdfCount = 0
    above90count = None
    above90percent = None
    for i in range(1, 1+maxRxCount):
        pdfCount = 0
        for l in links:
            if links[l]['rxCount'] == i:
                pdfCount += 1
        cdfCount += pdfCount
        if i*100./maxRxCount >= 90 and above90count == None:
          currCount = len(links)-cdfCount
          currStat = i*1./maxRxCount
          currWeight = (i*1./maxRxCount - 0.9) / (1./maxRxCount)
          above90count = currCount * (currWeight) + prevCount * (1-currWeight)
        prevCount = len(links)-cdfCount
        str = "%3u (%6.2f%%): %5u (%6.2f%%), %5u (%6.2f%%) -- %5u (%6.2f%%)" %(i, i*100./maxRxCount, pdfCount, pdfCount*100./len(links), cdfCount, cdfCount*100./len(links), len(links)-cdfCount, (len(links)-cdfCount)*100./len(links))
        print str
        probingFile.write("%s\n"%(str))
    
    extractedDutyCycle = extractData(parsed, "Duty Cycle", "%",
                lambda x: x['module'] == 'Duty Cycle' and x['id'] != SINK_ID and x['id'] in parsed['nodeIDs'],
                lambda x: x['info']['dutyCycle'],
                {'min': 0, 'max': 25},
                1)
    dutyCycle = extractedDutyCycle["global"]["avg"]
    extractedDutyCycleTx = extractData(parsed, "Tx Duty Cycle", "%",
                lambda x: x['module'] == 'Duty Cycle' and x['id'] != SINK_ID and x['id'] in parsed['nodeIDs'],
                lambda x: x['info']['dutyCycleTx'],
                {'min': 0, 'max': 25},
                1)
    dutyCycleTx = extractedDutyCycleTx["global"]["avg"] 
    
    if dutyCycle > 100:
      dutyCycle = 100
    str = "Overall statistics: %u/%u (%.2f), above 90%%: %u (%.2f%%), tx duty cycle %.4f%%, duty cycle %.4f%%" %(
            totalRxCount, totalTxCount, totalRxCount*1./totalTxCount,
            above90count, above90count*100./len(links),
            dutyCycleTx, dutyCycle
            )
    print str
    probingFile.write("%s\n"%(str))

def analyzeTimeline(dir, parsed):
        
    contentionlog = open(os.path.join(dir, "contentionlog.txt"), "w")
        
    txGraph = {}
    channelSet = []
        
    nodeIDs = parsed['nodeIDs']
    timeline = parsed['timeline']
    
    if len(timeline) == 0:
        return
    
    timeline_filtered = OrderedDict()
    for asn in timeline.keys():
        asnEvents = timeline[asn]
        for node in asnEvents:
            if asnEvents[node]['asnInfo']['slotframe'] >= 2:
                if not asn in timeline_filtered:
                    timeline_filtered[asn] = {}
                timeline_filtered[asn][node] = asnEvents[node]
        
    firstAsn = timeline.keys()[0]
    lastAsn = timeline.keys()[-1]

    for asn in range(firstAsn, lastAsn, 1):        
        if asn in timeline:
            # First count how many tx occured at this asn
            txCount = 0
            asnEvents = timeline[asn]
            for node in asnEvents:
                event = asnEvents[node]['event']
                if event == 'Tx':
                    if not node in txGraph:
                        txGraph[node] = {}
                    txCount += 1
                    #if not node in txGraph:
                        #txGraph[node] = {}
                    #print asnEvents[node]
            # Now look only at asn with a single tx
            #if txCount == 1:
            if txCount > 0:
                for node in asnEvents:
                    event = asnEvents[node]['event']
                    if event == 'Rx':    
                        sender = asnEvents[node]['prevHop']
                        channel = asnEvents[node]['asnInfo']['channel']
                        if not channel in channelSet:
                            channelSet.append(channel)
                        if not sender in txGraph:
                            txGraph[sender] = {}
                        if not node in txGraph[sender]:
                            txGraph[sender][node] = {}
                        if not channel in txGraph[sender][node]:
                            txGraph[sender][node][channel] = 0
                        txGraph[sender][node][channel] += 1
                        
    # print out connectivity graph
    nChannels = len(channelSet)
    for sender in txGraph:
        for receiver in txGraph[sender]:
            print "%u -> %u: %u"%(sender, receiver, len(txGraph[sender][receiver])), txGraph[sender][receiver],
            print ""
            
    totalRxAttempts = 0
    totalRxSuccess = 0
    totalRxAttemptsContended = 0
    totalRxSuccessContended = 0
    totalRxAttemptsContendedSet = {}
    totalRxSuccessContendedSet = {}
    
    timeline = timeline_filtered
    
    # Now dissect every tx: contention or not, capture or not
    for asn in range(firstAsn, lastAsn, 1):        
        if asn in timeline:
            asnEvents = timeline[asn]
            for node in asnEvents:
                event = asnEvents[node]['event']
                if event == 'Tx':
                    receiver = asnEvents[node]['nextHop']
                    channel = asnEvents[node]['asnInfo']['channel']
                    slotframe = asnEvents[node]['asnInfo']['slotframe']
                    expectedReceiverSet = []
                    # compute set of nodes expected to receive this
                    if receiver == 0:
                        for r in txGraph[node]:
                            if connected(txGraph, node, r, channel):
                                expectedReceiverSet.append(r)
                    else:
                        if connected(txGraph, node, receiver, channel):
                            expectedReceiverSet = [receiver]
                    totalRxAttempts += len(expectedReceiverSet)
                    # loop over all expected receivers 
                    for r in expectedReceiverSet:
                        success = r in asnEvents and asnEvents[r]['event'] == 'Rx' and asnEvents[r]['prevHop'] == node
                        if success:
                            totalRxSuccess += 1
                        # check other transmissions that are going on on this channel
                        contenderCount = 0
                        for contender in asnEvents:
                            if(contender != node and asnEvents[contender]['event'] == 'Tx'
                               and asnEvents[contender]['asnInfo']['channel'] == channel
                               and asnEvents[contender]['asnInfo']['slotframe'] == slotframe # TODO remove this hack
                               and connected(txGraph, contender, r, channel)):
                                contenderCount += 1
                        if contenderCount > 0:
                            if not contenderCount in totalRxAttemptsContendedSet:
                                totalRxAttemptsContendedSet[contenderCount] = 0
                                totalRxSuccessContendedSet[contenderCount] = 0
                            totalRxAttemptsContended += 1
                            totalRxAttemptsContendedSet[contenderCount] += 1
                            if success:
                                totalRxSuccessContended += 1
                                totalRxSuccessContendedSet[contenderCount] += 1
                            #print "%x, %u->%u, %u %u" %(asn, node, r, contenderCount, success)
    totalRxAttemptsNonContended = totalRxAttempts - totalRxAttemptsContended
    totalRxSuccessNonContended = totalRxSuccess - totalRxSuccessContended
    str = "Overall Rx statistics: %u/%u (%.4f%%)" %(totalRxSuccess, totalRxAttempts, 100.*totalRxSuccess/totalRxAttempts)
    print str
    contentionlog.write(str + "\n")
    str = "Overall Non-Contended Rx statistics: %u/%u (%.4f%%)" %(totalRxSuccessNonContended, totalRxAttemptsNonContended, 100.*totalRxSuccessNonContended/totalRxAttemptsNonContended)
    print str
    contentionlog.write(str + "\n")
    if totalRxAttemptsContended > 0:
        str = "Overall Contended Rx statistics: %u/%u (%.4f%%)" %(totalRxSuccessContended, totalRxAttemptsContended, 100.*totalRxSuccessContended/totalRxAttemptsContended)
        print str
        contentionlog.write(str + "\n")
    for contenderCount in totalRxAttemptsContendedSet:
        str = "%u Contenders Rx statistics: %u/%u (%.4f%%)" %(contenderCount+1,
                    totalRxSuccessContendedSet[contenderCount], totalRxAttemptsContendedSet[contenderCount],
                    100.*totalRxSuccessContendedSet[contenderCount]/totalRxAttemptsContendedSet[contenderCount])
        print str
        contentionlog.write(str + "\n")
    str = "Portion of successful Rx having contenders: %u/%u (%.4f%%)" %(totalRxSuccessContended, totalRxSuccess, 100.*totalRxSuccessContended/totalRxSuccess)
    print str
    contentionlog.write(str + "\n")
    str = "Portion of attempted Rx having contenders: %u/%u (%.4f%%)" %(totalRxAttemptsContended, totalRxAttempts, 100.*totalRxAttemptsContended/totalRxAttempts)
    print str
    contentionlog.write(str + "\n")
                                
def process(parsed):      
        global MIN_TIME, MAX_TIME, TARGET_MIN_TIME
        global scriptlog
                
        dataset = parsed['dataset']
        appDataStats = parsed['appDataStats']
        dir = parsed['dir']
        
        scriptlog = open(os.path.join(dir, "scriptlog.txt"), "w")
    
        destDir = os.path.join(dir, "data")
        if os.path.exists(destDir):
            for toRemove in os.listdir(destDir):
                os.remove(os.path.join(destDir,toRemove))
    
        #print "\nGenerating timeline txOnly=True"
        #generateTimelineFile(dir, parsed, txOnly=True)
    
        extractedPRRfullDuration = extractData(parsed, "End-to-end Delivery Ratio", "%",
                lambda x: x['module'] == 'App' and 'sending' in x['info'],
                lambda x: 100 if x['info']['received'] else 0,
                {'min': 1, 'max': 100},
                MIN_INTERVAL, verbose=False, export=False)
        
        if parsed['maxTime']/60 > TARGET_MIN_TIME + 1:
            #MIN_TIME = (parsed['maxTime']/60) / 2
            MIN_TIME = TARGET_MIN_TIME
            MAX_TIME = (parsed['maxTime']/60) - 1
        else:
            MIN_TIME = 0
            MAX_TIME = parsed['maxTime']/60
            
        allPlottableData = []
    
        extractedPRR = extractData(parsed, "End-to-end Delivery Ratio", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if x['info']['received'] else 0,
                        {'min': 100, 'max': 100},
                        MIN_INTERVAL, revert=True)
        allPlottableData.append( 
            extractedPRR    
        )

        allPlottableData.append( 
            extractData(parsed, "MAC drop", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received'] and "macDrop" in x['info'] else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "MAC queue full", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received'] and "macError" in x['info'] else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "No route found", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received'] and "noRouteFound" in x['info'] else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "FW Error", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received'] and "fwError" in x['info'] else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Nbr Cache Miss or Incomplete", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received'] and "nbrCachePb" in x['info'] else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Link-layer duplicate", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received']
                          and not "macDrop" in x['info']
                          and not "macError" in x['info']
                          and not "noRouteFound" in x['info']
                          and not "fwError" in x['info']
                          and not "nbrCachePb" in x['info']
                          and "lldup" in x['info']
                          else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Not received, not dropped", "%",
                        lambda x: x['module'] == 'App' and 'sending' in x['info'],
                        lambda x: 100 if not x['info']['received']
                          and not "macDrop" in x['info']
                          and not "macError" in x['info']
                          and not "noRouteFound" in x['info']
                          and not "fwError" in x['info']
                          and not "nbrCachePb" in x['info']
                          and not "lldup" in x['info']
                          else 0,
                        {'min': 0, 'max': 0},
                        MIN_INTERVAL, verbose=True, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Latency", "s",
                        lambda x: x['module'] == 'App' and 'latency' in x['info'],
                        lambda x: x['info']['latency'],
                        {'min': 0, 'max': 10},
                        MIN_INTERVAL, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Hop Count", "#",
                        lambda x: x['module'] == 'App' and x['info']['event']=='sending' and x['info']['hops']!=-1,
                        lambda x: x['info']['hops'],
                        {'min': 0, 'max': 10},
                        MIN_INTERVAL, revert=True)
        )
        allPlottableData.append( 
            extractData(parsed, "Rank", "etx",
                        lambda x: x['module'] == 'RPL' and x['info']['event']=='status',
                        lambda x: x['info']['rank'],
                        {'min': 0, 'max': 0xffff},
                        MIN_INTERVAL)
        )
        allPlottableData.append( 
            extractData(parsed, "DIO interval", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event']=='status',
                        lambda x: x['info']['dioint'],
                        {'min': 0, 'max': 0xffff},
                        MIN_INTERVAL)
        )
        allPlottableData.append( 
            extractData(parsed, "Neighbor count", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event']=='status',
                        lambda x: x['info']['neighbors'],
                        {'min': 0, 'max': 30},
                        MIN_INTERVAL)
        )
        #allPlottableData.append( 
         #   extractData(parsed, "TSCH Unicast Tx Has Contenders", "%",
          #              lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
           #             lambda x: 100 if x['info']['contenderCount'] > 0 else 0,
            #            {'min': 0, 'max': 100},
             #           MIN_INTERVAL)
        #)
        allPlottableData.append( 
            extractData(parsed, "MAC Unicast Count", "#",
                        lambda x: (x['module'] == 'TSCH' or x['module'] == 'Cmac' or x['module'] == 'Nullrdc')
                            and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
                        lambda x: 1,
                        {'min': 1, 'max': 1},
                        MIN_INTERVAL, doSum=True)
        )
        allPlottableData.append( 
            extractData(parsed, "MAC Unicast Success", "%",
                        lambda x: (x['module'] == 'TSCH' or x['module'] == 'Cmac' or x['module'] == 'Nullrdc')
                            and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
                        lambda x: 100 if x['info']['status'] == 0 else 0,
                        {'min': 0, 'max': 100},
                        MIN_INTERVAL, debug=True)
        )
        #allPlottableData.append( 
        #    extractData(parsed, "TSCH Unicast ACK Lost", "%",
         #               lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
          #              lambda x: 100 if x['info']['status'] == 2 and x['info']['rxCount'] > 0 else 0,
                        #{'min': 0, 'max': 100},
                        #MIN_INTERVAL)
        #)
        #allPlottableData.append( 
         #   extractData(parsed, "TSCH Unicast lost (no contender)", "%",
          #              lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
           #             lambda x: 100 if x['info']['status'] == 2 and x['info']['rxCount'] == 0 and x['info']['contenderCount'] == 0 else 0,
            #            {'min': 0, 'max': 100},
             #           MIN_INTERVAL)
        #)
        #allPlottableData.append( 
         #   extractData(parsed, "TSCH Unicast lost (with contenders)", "%",
          #              lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx' and x['info']['is_unicast'],
           #             lambda x: 100 if x['info']['status'] == 2 and x['info']['rxCount'] == 0 and x['info']['contenderCount'] > 0 else 0,
            #            {'min': 0, 'max': 100},
             #           MIN_INTERVAL)
        #)
        #allPlottableData.append( 
         #   extractData(parsed, "TSCH Unicast Mean TxCount", "#",
          #              lambda x: x['module'] == '6LoWPAN' and x['info']['event'] == 'sent'  and x['info']['is_unicast'],
           #             lambda x: x['info']['txCount'],
            #            {'min': 0, 'max': 16},
             #           MIN_INTERVAL),
        #)
        allPlottableData.append( 
            extractData(parsed, "TSCH Unicast PRR", "%",
                        lambda x: x['module'] == '6LoWPAN' and x['info']['event'] == 'sent'  and x['info']['is_unicast'],
                        lambda x: 1./x['info']['txCount'] if x['info']['status'] == 0 else 0,
                        {'min': 0, 'max': 16},
                        MIN_INTERVAL,
                        weightFunction=(lambda x: x['info']['txCount'])),
        )
        allPlottableData.append( 
            extractData(parsed, "6LoWPAN Unicast Success", "%",
                        lambda x: x['module'] == '6LoWPAN' and x['info']['event'] == 'sent',
                        lambda x: 100 if x['info']['status'] == 0 else 0,
                        {'min': 0, 'max': 100},
                        MIN_INTERVAL)
        ) 
        allPlottableData.append( 
            extractData(parsed, "Duty Cycle", "%",
                        lambda x: x['module'] == 'Duty Cycle' and x['id'] != SINK_ID and x['id'] in parsed['nodeIDs'],
                        lambda x: x['info']['dutyCycle'],
                        {'min': 0, 'max': 25},
                        1),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL trickle reset", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'trickleReset',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL parent switch", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'parentSwitch',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL DAO Sent", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'sendingDAO',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL No-Path DAO Sent", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'sendingNoPathDAO',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL MC DIO Sent", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'sendingMcDIO',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL UC DIO Sent", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'sendingUcDIO',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
        allPlottableData.append( 
            extractData(parsed, "RPL MC DIS Sent", "#",
                        lambda x: x['module'] == 'RPL' and x['info']['event'] == 'sendingDIS',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH different time sources had", "#",
#                        lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'updateTimeSource',
#                        lambda x: x['info']['timeSourceCount'],
#                        {'min': 0, 'max': 5},
#                        1, doMax=True),
#        )
        allPlottableData.append( 
            extractData(parsed, "TSCH has lost synch!", "#",
                        lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'resync',
                        lambda x: 1,
                        {'min': 0, 'max': 1},
                        1, doSum=True),
        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH links used for Tx", "%",
#                        lambda x: x['module'] == 'Duty Cycle' and 'txLinksPercent' in x['info'],
#                        lambda x: x['info']['txLinksPercent'],
#                        {'min': 0, 'max': 100},
#                        1),
#        )
#       allPlottableData.append( 
#            extractData(parsed, "TSCH links used for Rx", "%",
#                        lambda x: x['module'] == 'Duty Cycle' and 'rxLinksPercent' in x['info'],
#                        lambda x: x['info']['rxLinksPercent'],
#                        {'min': 0, 'max': 100},
#                        1),
#        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH EB sent", "#",
#                        lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx'
#                            and not x['info']['is_unicast'] and not x['info']['is_data'],
#                        lambda x: 1,
#                        {'min': 0, 'max': 1},
#                        1, doSum=True),
#        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH Keek-alive sent", "#",
#                        lambda x: x['module'] == 'TSCH' and x['info']['event'] == 'Tx'
#                            and x['info']['is_unicast'] and not x['info']['is_data'],
#                        lambda x: 1,
#                        {'min': 0, 'max': 1},
#                        1, doSum=True),
#        )
        allPlottableData.append( 
            extractData(parsed, "TSCH mean drift", "PPM",
                        lambda x: x['module'] == 'TSCH' and 'drift' in x['info'],
                        lambda x: ((abs(x['info']['drift']) / float(x['info']['drift_dt'])) / TICK_PER_LINK) * 1000000,
                        {'min': 0, 'max': 160},
                        1),
        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH max drift rate", "PPM",
#                        lambda x: x['module'] == 'TSCH' and 'drift' in x['info'],
#                        lambda x: ((abs(x['info']['drift']) / float(x['info']['drift_dt'])) / TICK_PER_LINK) * 1000000,
#                        {'min': 0, 'max': 160},
#                        1, doMax=True),
#        )
        allPlottableData.append( 
            extractData(parsed, "TSCH max abs drift", "us",
                        lambda x: x['module'] == 'TSCH' and 'drift' in x['info'],
                        lambda x: abs(x['info']['drift']) * TICK_DURATION * 1000000,
                        {'min': 0, 'max': 1000},
                        1, doMax=True),
        )
#        allPlottableData.append( 
#            extractData(parsed, "TSCH mean drift correction interval", "s",
#                        lambda x: x['module'] == 'TSCH' and 'drift' in x['info'],
#                        lambda x: x['info']['drift_dt'] * LINK_DURATION,
#                        {'min': 0, 'max': 100},
#                        1),
#        )

        summaryFile = open(os.path.join(dir, "summary.txt"), 'w')        
        str = "\nRun duration: %d min" %(parsed['maxTime']/60)
        print str
        summaryFile.write("%s\n" %str)
        str = "Nodes active: %d/%d" %(len(parsed['nodeIDs']), len(parsed['allNodeIDs']))
        print str
        summaryFile.write("%s\n" %str)
        if len(parsed['inactiveNodeIDs']) > 0:
          str = "List of nodes inactive (num: %u): %s"%(len(parsed['inactiveNodeIDs']), "".join(map(lambda x: "%u, " %x,parsed['inactiveNodeIDs'])))
          print str
          summaryFile.write("%s\n" %str)
        if len(parsed['ignoredNodeIDs']) > 0:
          str = "List of nodes ignored (num: %u): %s"%(len(parsed['ignoredNodeIDs']), "".join(map(lambda x: "%u, " %x,parsed['ignoredNodeIDs'])))
          print str
          summaryFile.write("%s\n" %str)
        str = "App messages in period [%d-%d]: %d/%d (loss rate: 1/%d)" %(MIN_TIME, MAX_TIME, extractedPRR['validCount'], extractedPRR['count'],
          extractedPRR['count']/(extractedPRR['count']-extractedPRR['validCount']) if extractedPRR['count'] != extractedPRR['validCount'] else 0)
        print str
        summaryFile.write("%s\n" %str)
        str = "App messages in period [%d-%d]: %d/%d" %(0, parsed['maxTime']/60, extractedPRRfullDuration['validCount'], extractedPRRfullDuration['count'])
        print str
        summaryFile.write("%s\n" %str)

        str = "\nGlobal statistics:"
        print str
        summaryFile.write("%s\n" %str)
        
        for plottableData in allPlottableData:
            globalData = plottableData['global']
            str = "%48s "%(plottableData['name'])

            if plottableData['doSum']:
                str += showStats(plottableData['perNodeGlobal'], plottableData['unit'])
                if 'sum' in plottableData['perNodeGlobal']:
                    str += "   Sum: %8.2f " %(plottableData['perNodeGlobal']['sum'])
                str += showStatsMinMax(plottableData['perNodeGlobal'])
            elif plottableData['doMax']:
                str += showStats(plottableData['perNodeGlobal'], plottableData['unit'])
                str += "   Max: %8.2f " %(plottableData['perNodeGlobal']['max'])
                str += showStatsMinMax(plottableData['perNodeGlobal'])
            else:
                str += showStats(plottableData['global'], plottableData['unit'])
                str += "   Med: %8.2f " %(plottableData['global']['p50'])
                str += showStatsMinMax(plottableData['global'])

            print str
            summaryFile.write("%s\n" %str) 
        print ""
            
            
def main():
    global MIN_TIME
    global MAX_TIME
    global TARGET_MIN_TIME

    if len(sys.argv) < 2:
        dir = '.'
    else:
        dir = sys.argv[1].rstrip('/')

    minTimeFile = os.path.join(dir, "mintime.txt")
    if os.path.isfile(minTimeFile):
        TARGET_MIN_TIME = int(open(minTimeFile, "r").readlines()[0])
        print "Target min time: %u minutes" %(TARGET_MIN_TIME)

    file = os.path.join(dir, "log.txt")
    parsed = parseLogs.doParse(file, SINK_ID)
    parsed['dir'] = dir
    
 
    
    print "\nProcessing %s" %(file)
    process(parsed)
        
#        print "\nGenerating vector graphics timeline"
 #       generateTimelineFileVector(dir, parsed, txOnly=False)
        
#        print "\nGenerating timeline txOnly=False"
 #       generateTimelineFile(dir, parsed, txOnly=False)
 
   #     print "\nGenerating timeline txOnly=True"
    #    generateTimelineFile(dir, parsed, txOnly=True)
     #   
    #print "\nAnalyzing timeline"
    #analyzeTimeline(dir, parsed)
 
#        print "\nExtracting probing data"
 #       extractProbing(dir, parsed)
                
main()
