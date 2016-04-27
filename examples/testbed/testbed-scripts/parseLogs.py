#!/usr/bin/env python

import re
import fileinput
import math
from sets import Set
from pylab import *
from collections import OrderedDict

appDataStats = {}
receivedList = {}
droppedList = {}
nodeState = {}
timeline = OrderedDict()

def dumpTxStats():
    for id in nodeState:
        for nbr in nodeState[id]['txStats']:
            if nodeState[id]['txStats'][nbr]['txCount'] > 1:
                rssi = average(nodeState[id]['txStats'][nbr]['rssi'])
                lqi = average(nodeState[id]['txStats'][nbr]['lqi'])
                minRssi = min(nodeState[id]['txStats'][nbr]['rssi'])
                minLqi = min(nodeState[id]['txStats'][nbr]['lqi'])
                maxRssi = max(nodeState[id]['txStats'][nbr]['rssi'])
                maxLqi = max(nodeState[id]['txStats'][nbr]['lqi'])
                link = nodeState[id]['txStats'][nbr]['link']
                print nodeState[id]['txStats'][nbr]['rssi']
                txCount = nodeState[id]['txStats'][nbr]['txCount']
                ackCount = nodeState[id]['txStats'][nbr]['ackCount']
                prr = float(ackCount) / txCount
                print "TxStats: %u -> %u : rssi %u %u %u lqi %u %u %u link-metric %u PRR %.3f (%u/%u)"%(id, nbr, rssi, minRssi, maxRssi, lqi, minLqi, maxLqi, link, prr, ackCount, txCount)
    
def doAppDrop(packetId, status):
    global appDataStats, receivedList
    droppedList[packetId] = {'status': status}
    if packetId in appDataStats:    
      appDataStats[packetId]['refmoduleInfo'][status] = True 

def doAppReceive(packetId, recvTime, hops):
    global appDataStats, receivedList
    receivedList[packetId] = {'recvTime': recvTime, 'hops': hops}
    if packetId in appDataStats:
      latency = max(0, recvTime - appDataStats[packetId]['sendTime']);
      appDataStats[packetId]['refmoduleInfo']['received'] = True
      if not 'latency' in appDataStats[packetId]['refmoduleInfo'] or latency < appDataStats[packetId]['refmoduleInfo']['latency']:
        appDataStats[packetId]['refmoduleInfo']['latency'] = latency 
      if receivedList[packetId]['hops'] > appDataStats[packetId]['refmoduleInfo']['hops']:
        appDataStats[packetId]['refmoduleInfo']['hops'] = receivedList[packetId]['hops']

def tsch_register_drift(line, id, timeSource, asn, drift):
    global nodeState
    moduleInfo = {}
    if nodeState[id]['lastSyncAsn']:
        drift_dt = asn - nodeState[id]['lastSyncAsn']
        if drift_dt > 0:
            moduleInfo = { 'drift': drift, 'drift_dt': drift_dt }                       
        else :
            print "Warning: drift_dt <= 0", line
    nodeState[id]['lastSyncAsn'] = asn
    return moduleInfo

def parseDutyCycle(line, time, id, log, packetInfo, asnInfo):
    global nodeState
    res = re.compile('^\[(\d+) (\d+)\]\\s*(\d+)\\s*\+\\s*(\d+)\\s*/\\s*(\d+)').match(log)
    if res:
        currentId = int(res.group(1))
        if currentId != id:
            print 'Warning: node %d has current nodeid %d!' %(id, currentId)
        moduleInfo = {'count': int(res.group(2)), 'tx': int(res.group(3)), 'rx': int(res.group(4)), 't': int(res.group(5))}
        if float(moduleInfo['t']) > 0: 
          moduleInfo['dutyCycle'] = 100 * (moduleInfo['tx'] + moduleInfo['rx']) / float(moduleInfo['t'])
          moduleInfo['dutyCycleTx'] = 100 * (moduleInfo['tx']) / float(moduleInfo['t'])
          moduleInfo['dutyCycleRx'] = 100 * (moduleInfo['rx']) / float(moduleInfo['t'])
        else:      
          moduleInfo['dutyCycle'] = 0
          moduleInfo['dutyCycleTx'] = 0
          moduleInfo['dutyCycleRx'] = 0
        
        if nodeState[id]['totalLinks'] > 0:
            moduleInfo['txLinksPercent'] = nodeState[id]['txLinks'] * 100. /  nodeState[id]['totalLinks']
            moduleInfo['rxLinksPercent'] = nodeState[id]['rxLinks'] * 100. /  nodeState[id]['totalLinks']

        nodeState[id]['txLinks'] = 0
        nodeState[id]['rxLinks'] = 0
        nodeState[id]['totalLinks'] = 0
                
        return moduleInfo
    return None

def parseApp(line, time, id, log, packetInfo, asnInfo):
    global appDataStats, receivedList

    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']
    else:
        return None
    
#---- App: Sending -------------------------------------------------------------------------------------------------------------
    if log.startswith('sending'):
        moduleInfo = {'event': 'sending', 'sending': True, 'received': False,
            'receivedCount': 0, 'fwCount': 0,  
            'hops': -1, 
            'lastLine': line,
            'e2eRxCount': 0}

        # update packet data
        if not packetId in appDataStats:
            appDataStats[packetId] = {}
        else:
            if 'needsAdding' in appDataStats[packetId]:
                moduleInfo.update(appDataStats[packetId]['needsAdding'])
                        
        appDataStats[packetId].update({'sendTime': time, 'refmoduleInfo': moduleInfo, 'line': line})

        # we have received or dropped this packet earlier
        if packetId in receivedList:
            doAppReceive(packetId, receivedList[packetId]['recvTime'], receivedList[packetId]['hops'])
        if packetId in droppedList:
            doAppDrop(packetId, droppedList[packetId]['status'])
        return moduleInfo
                
#---- App: Received -------------------------------------------------------------------------------------------------------------
    elif log.startswith('received'):
        moduleInfo = {'event': 'received', 'recvTime': time, 'hops': hop }
        doAppReceive(packetId, time, hop)
        return moduleInfo 
    
    return None

def parseRPL(line, time, id, log, packetInfo, asnInfo):
    global nodeState

#---- RPL: parent switch -------------------------------------------------------------------------------------------------------------    
    if log.startswith("parent switch"):
        return {'event': 'parentSwitch'}

#---- RPL: DAO -------------------------------------------------------------------------------------------------------------    
    if "Sending a DAO with" in log:
        return {'event': 'sendingDAO'}

#---- RPL: No-Path DAO -------------------------------------------------------------------------------------------------------------
    if "Sending a No-Path DAO with" in log:
        return {'event': 'sendingNoPathDAO'}

#---- RPL: multicast DIO -------------------------------------------------------------------------------------------------------------        
    if "Sending a multicast-DIO" in log:
        return {'event': 'sendingMcDIO'}
        
#---- RPL: unicast DIO -------------------------------------------------------------------------------------------------------------        
    if "Sending unicast-DIO" in log:
        return {'event': 'sendingUcDIO'}

#---- RPL: DIS -------------------------------------------------------------------------------------------------------------        
    if "Sending a DIS" in log:
        return {'event': 'sendingDIS'}

#---- RPL: no route found -------------------------------------------------------------------------------------------------------------    
    if "no route found" in log:
        if packetInfo != None:
        	doAppDrop(packetInfo['id'], 'noRouteFound')
        return {'event': 'noRouteFound'}
    
#---- RPL: fw error -------------------------------------------------------------------------------------------------------------    
    if "Rank error signalled in RPL option" in log:
        if packetInfo != None:
            doAppDrop(packetInfo['id'], 'fwError')
        return {'event': 'fwError'}
    
#---- RPL: state overview  -------------------------------------------------------------------------------------------------------------
    res = re.compile('MOP \d OCP \d rank (\d+) dioint (\d+), nbr count (\d+)').match(log)
    if res:             
        rank = int(res.group(1))
        dioint = int(res.group(2))
        neighbors = int(res.group(3))
        return {'event': 'status', 'rank': rank, 'dioint': dioint, 'neighbors': neighbors }
    
#---- RPL: neighbor information -------------------------------------------------------------------------------------------------------------  
    res = re.compile('nbr\s*(\d+)\s*(\d+)\s*,\s*(\d+)\s*=>\s*(\d+)[\s\*]*\(rssi ([-\d]+) lqi (\d+)\)').match(log)
    if res:
        nbr = int(res.group(1))
        diorank = int(res.group(2))
        link = int(res.group(3))
        rank = int(res.group(4))
        rssi = int(res.group(5))
        lqi = int(res.group(6))

        if not nbr in nodeState[id]['txStats']:
            nodeState[id]['txStats'][nbr] = {'txCount': 0, 'ackCount': 0, 'rssi': [], 'lqi': []}
        nodeState[id]['txStats'][nbr]['link'] = link
        nodeState[id]['txStats'][nbr]['rssi'].append(rssi)
        nodeState[id]['txStats'][nbr]['lqi'].append(lqi)
           
        return None
    
    return None

def parseTcpip(line, time, id, log, packetInfo, asnInfo):
    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']
        
    if "nbr cache" in log:
        if packetInfo != None:
        	doAppDrop(packetInfo['id'], 'nbrCachePb')
        return {'event': 'nbrCachePb' }
    if "no route" in log:
        if packetInfo != None:
        	doAppDrop(packetInfo['id'], 'noRouteFound')
        return {'event': 'noRouteFound'}

    res = re.compile('fw to (\d+)').match(log) 
    if res:             
        nextHop = int(res.group(1))           
        return {'event': 'fw', 'nextHop': nextHop }
    return None

def parse6lowpan(line, time, id, log, packetInfo, asnInfo):
    global appDataStats, receivedList, nodeState
    
    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']
    
#---- 6LoWPAN: input -------------------------------------------------------------------------------------------------------------
    res = re.compile('uc input from (\d+)').match(log)
    if res:             
        prevHop = int(res.group(1)) 
                         
        if packetInfo and id == dst:
          doAppReceive(packetId, time, hop)

        return {'event': 'input', 'from': prevHop }

#---- 6LoWPAN: packet sent -------------------------------------------------------------------------------------------------------------
    res = re.compile('uc sent to (\d+), st (\d+) (\d+)').match(log)                     
    if res:
        nextHop = int(res.group(1))
        status = int(res.group(2))
        txCount = int(res.group(3))
        
        if packetInfo and status != 0: 
            doAppDrop(packetId, 'macDrop' if status == 2 else 'macError')
                         
        if not nextHop in nodeState[id]['txStats']:
            nodeState[id]['txStats'][nextHop] = {'txCount': 0, 'ackCount': 0, 'rssi': [], 'lqi': []}
        nodeState[id]['txStats'][nextHop]['txCount'] += txCount
        if status == 0:
            nodeState[id]['txStats'][nextHop]['ackCount'] += 1 
        
        return {'event': 'sent', 'to': nextHop, 'status': status, 'txCount': txCount, 'is_unicast': True }

    return None

def parseCmac(line, time, id, log, packetInfo, asnInfo):
    global appDataStats, receivedList, nodeState, timeline
    
    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']

#---- Cmac Tx -------------------------------------------------------------------------------------------------------------                
        res = re.compile('([ub]c) (\d+) tx (\d+), s (\d+) st (\d+)$').match(log)
        if res:
            is_unicast = res.group(1) == "uc"
            datalen = int(res.group(2))
            nextHop = int(res.group(3))
            strobeLen = int(res.group(4))
            status = int(res.group(5))
        
            return {'event': 'Tx', 'is_unicast': is_unicast, 'strobeLen': strobeLen,
                                 'datalen': datalen, 'nextHop': nextHop, 'packet': packetInfo,
                                 'status': status }
def parseNullrdc(line, time, id, log, packetInfo, asnInfo):
    global appDataStats, receivedList, nodeState, timeline
    
    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']

#---- Nullrdc Tx -------------------------------------------------------------------------------------------------------------                
        res = re.compile('([ub]c) (\d+) tx (\d+), st (\d+)$').match(log)
        if res:
            is_unicast = res.group(1) == "uc"
            datalen = int(res.group(2))
            nextHop = int(res.group(3))
            status = int(res.group(4))
        
            return {'event': 'Tx', 'is_unicast': is_unicast,
                                 'datalen': datalen, 'nextHop': nextHop, 'packet': packetInfo,
                                 'status': status }
                  
def parseTsch(line, time, id, log, packetInfo, asnInfo):
    global appDataStats, receivedList, nodeState, timeline
    
    if packetInfo != None:
        packetId = packetInfo['id']
        hop = packetInfo['hop']
        src = packetInfo['src']
        dst = packetInfo['dst']
    #else:
        #if asnInfo != None:
            #return None
    
    if asnInfo != None:
        asn = asnInfo['asn'] 
        slotframe = asnInfo['slotframe']
        timeslot = asnInfo['timeslot']
        channel_offset = asnInfo['channel_offset']
        channel = asnInfo['channel']
                
#---- TSCH link: Rx -------------------------------------------------------------------------------------------------------------
        res = re.compile('([ub]c)-([01])-[01] (\d+) rx (\d+)(.*)$').match(log)

        if res:
            is_unicast = res.group(1) == "uc"
            is_data = int(res.group(2))
            datalen = int(res.group(3))
            prevHop = int(res.group(4))
            endOfLog = res.group(5)
                                    
            moduleInfo = {'event': 'Rx', 'is_unicast': is_unicast, 'is_data': is_data,
                                 'datalen': datalen, 'prevHop': prevHop, 'packet': packetInfo, 'asnInfo': asnInfo }
            
            res = re.compile(', dr ([-\d]+)').match(endOfLog)
            if res:
                drift = int(res.group(1))
                moduleInfo.update(tsch_register_drift(line, id, prevHop, asn, drift))
            
            nodeState[id]['rxLinks'] += 1
            if nodeState[id]['lastLinkAsn']:
                nodeState[id]['totalLinks'] += (asn - nodeState[id]['lastLinkAsn']) / 3
            
            nodeState[id]['lastLinkAsn'] = asn
                                    
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
            nextHop = int(res.group(4))
            status = int(res.group(5))
            txCount = int(res.group(6))
            endOfLog = res.group(7)
        
            moduleInfo = {'event': 'Tx', 'is_unicast': is_unicast, 'is_data': is_data,
                                 'datalen': datalen, 'nextHop': nextHop, 'packet': packetInfo,
                                 'status': status, 'txCount': txCount, 'asnInfo': asnInfo }
                    
            #if packetInfo and status != 0:
                #doAppDrop(packetId, 'macDrop' if status == 2 else 'macError')
                    
            res = re.compile(', dr ([-\d]+)').match(endOfLog)
            if res:
                drift = int(res.group(1))
                moduleInfo.update(tsch_register_drift(line, id, nextHop, asn, drift))
                    
            nodeState[id]['txLinks'] += 1
            if nodeState[id]['lastLinkAsn']:
                nodeState[id]['totalLinks'] += (asn - nodeState[id]['lastLinkAsn']) / 3                                    
            nodeState[id]['lastLinkAsn'] = asn
               
            if not asn in timeline:
                timeline[asn] = OrderedDict()
            timeline[asn][id] = moduleInfo
                                            
            return moduleInfo
                
    else: # no asnInfo
                    
#---- TSCH: input -------------------------------------------------------------------------------------------------------------
        res = re.compile('received from (\d+)').match(log)
        if res:             
            prevHop = int(res.group(1)) 
                
            if packetInfo and id == dst:
                if packetId in appDataStats and appDataStats[packetId]['refmoduleInfo']['received'] == False:
                 doAppReceive(packetId, time, hop)
                 
            return {'event': 'input', 'from': prevHop }

#---- TSCH: re-synchronizing -------------------------------------------------------------------------------------------------------------
        if log == "leaving the network":
            return {'event': 'resync'}        

#---- TSCH: sending keep-alive -------------------------------------------------------------------------------------------------------------
        res = re.compile('sending KA to (\d+)').match(log)
        if res:             
            nextHop = int(res.group(1))
            return {'event': 'sendingKA'}

#---- TSCH: sending EB -------------------------------------------------------------------------------------------------------------
        if log == "enqueue EB packet":           
            return {'event': 'sendingEB'}
        
#---- TSCH: new time source -------------------------------------------------------------------------------------------------------------
        res = re.compile('update time source: (\d+) -> (\d+)').match(log)
        if res:             
            old = int(res.group(1))
            new = int(res.group(2))
            nodeState[id]['timeSources'].add(new)
            return {'event': 'updateTimeSource', 'timeSourceCount': len(nodeState[id]['timeSources'])}

    return None

################################################################################################################################
   
def parseLine(line):
    #res = re.compile('^(\d+)\\tID:(\d+)\\t(.*)$').match(line)
    #if res:
    #    return int(res.group(1)), int(res.group(2)), res.group(3)
    res = re.compile('^(\d+)\.(\d+);m3-(\d+);(.*)$').match(line)
    if res:
        return int(res.group(1)) * 1000000 + int(res.group(2)), int(res.group(3)), res.group(4)
    return None, None, None

def doParse(file, sinkId):
    global appDataStats, hopDataStats, receivedList, nodeState, timeline

    allData = []
    baseTime = None
    lastPrintedTime = 0
    time = None
    nodeIDs = []
    allNodeIDs = []
    nonExtractedModules = []
    parsingFunctions = {
                        #'Duty Cycle': parseDutyCycle,
                        'App': parseApp,
                        'RPL': parseRPL,
                        'Tcpip': parseTcpip,
                        '6LoWPAN': parse6lowpan,
                        'TSCH': parseTsch,
                        #'Cmac': parseCmac,
                        #'Nullrdc': parseNullrdc,
                        #'Scheduler': None,
                        }
    
    linesProcessedCount = 0
    linesParsedCount = 0
    
    for line in open(file, 'r').readlines():
    #for line in open(file, 'r').readlines()[:1000000]:
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
            #if not baseTime:
            #    baseTime = time
            #time -= baseTime
            #time /= 1000000. # time from us to s 
        else:           
            # default for all structures
            packetInfo = None
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
            
            if not id in nodeState:
                nodeState[id] = {'timeSources': Set(), 'lastSyncAsn': None, 
                'rxLinks': 0, 'txLinks': 0, 'totalLinks': 0, 'lastLinkAsn': None,
                'txStats': {}}
                     
            # match lines that include packet info
            res = re.compile('^([^\[]*) \[([a-f\d]+) (\d+) (\d+)->(\d+)\]').match(log)
            if res:
                #print line
                log = res.group(1)
                packetId = int(res.group(2), 16)
                hop = int(res.group(3))
                src = int(res.group(4))
                dst = int(res.group(5))
                packetInfo = {'id': packetId, 'hop': hop, 'src': src, 'dst': dst}
                
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

            linesProcessedCount += 1
            # process each module separately
            if not module in parsingFunctions:
                #print "Module unknown: ", line
                continue
            
            parsingFunction = parsingFunctions[module]                
            if parsingFunction != None:
                moduleInfo = parsingFunction(line, time, id, log, packetInfo, asnInfo)
                
            # keep track of the last line seen for every packet   
            if packetInfo and packetId in appDataStats and 'refmoduleInfo' in appDataStats[packetId]:
                appDataStats[packetId]['refmoduleInfo']['lastLine'] = line
            
            #if asnInfo != None:
                #continue

            if not id in allNodeIDs:
                allNodeIDs.append(id)
            if module == "App":
                if not id in nodeIDs:
                    nodeIDs.append(id)

            if moduleInfo == None:
                #print "Could not parse: ", line
                continue
    
            linesParsedCount += 1
            lineData = {'time': time, 'id': id, 'module': module, 'log': log, 'packet': packetInfo, 'info': moduleInfo}
            allData.append(lineData)
                
    #nodeIDs = allNodeIDs
    nodeIDs.sort()
    nodeIDs = filter(lambda x: x!=sinkId, nodeIDs)
    nodeIDs.insert(0, sinkId)
    
    for asn in timeline:
        for senderId in timeline[asn]:
            rxCount = 0
            contenderCount = 0
            for id in timeline[asn]:
                senderInfo = timeline[asn][senderId]
                info = timeline[asn][id]
                if senderInfo['asnInfo']['channel'] == info['asnInfo']['channel']:
                    if info['event'] == 'Rx' and info['prevHop'] == senderId:
                        rxCount += 1
                    if info['event'] == 'Tx' and id != senderId:
                        contenderCount += 1
        
            timeline[asn][senderId]['rxCount'] = rxCount
            timeline[asn][senderId]['contenderCount'] = contenderCount
    
    print "\nParsed %d/%d lines" %(linesParsedCount, linesProcessedCount)
    for id in filter(lambda x: x not in nodeIDs, allNodeIDs):
        print "Warning: node %u was not active" %id
        
    #dumpTxStats()
    
    return {'file': file, 'dataset': allData, 'maxTime': time, 'nodeIDs': nodeIDs, 'allNodeIDs': allNodeIDs, 'appDataStats': appDataStats, 'timeline': timeline}
