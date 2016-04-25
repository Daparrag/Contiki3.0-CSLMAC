#!/usr/bin/env python

import MySQLdb
import os
import sys
import time
import datetime

username = 'simonduq'

def extractRun(runName):
    global cHandler
    
    #longName = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M") + "_Indriya_" + runName

    outputFilePath = os.path.join("experiments", "Indriya_" + runName, "log.txt")
    ongoingFilePath = os.path.join("experiments", "Indriya_" + runName, "ongoing")
    outputDir = os.path.dirname(outputFilePath)
    if os.path.exists(outputFilePath) and not os.path.exists(ongoingFilePath):
        print "Indriya trace %s already extracted" %(runName)    
        return

    print "Extracting Indriya trace %s" %(runName)
    if not os.path.exists(outputDir):
        os.makedirs(outputDir)
    open(ongoingFilePath, 'w').close()
    
    logFile = open(outputFilePath, "w")
    
    baseTime = None

    totalRows = 0         
    while True:
        
        count = cHandler.execute("SELECT  LEFT(`msg`, 256),  `insert_time`,  `motelabMoteID`,  `milli_time`,  `motelabSeqNo` FROM `%s`.`%s` LIMIT %d, 10000;" %(username, runName, totalRows))
        
        if count == 0:
            time.sleep(60)
            count = cHandler.execute("SELECT  LEFT(`msg`, 256),  `insert_time`,  `motelabMoteID`,  `milli_time`,  `motelabSeqNo` FROM `%s`.`%s` LIMIT %d, 10000;" %(username, runName, totalRows))
            if count == 0:
                print "End of trace"
                os.remove(ongoingFilePath)
                return
        
        totalRows += count
        print totalRows,
        sys.stdout.flush()
        
        for r in cHandler.fetchall():
            payload = r[0]
            nodeId = r[2] - 40000
            t = r[3]
            if baseTime == None:
                baseTime = t
            timestamp = (r[3] - baseTime) * 1000
            cleanPayload = ""
            for i in range(1, len(payload), 2):
                if payload[i] == '\0':
                    break
                cleanPayload += payload[i]
            logFile.write("%012u\tID:%u\t%s" %(timestamp, nodeId, cleanPayload))
            logFile.flush()
            
        if count < 10000:
            time.sleep(1)

while True:
    myDB = MySQLdb.connect(host="indriya.comp.nus.edu.sg", port=3306, user=username, passwd="04indriya04")
    cHandler=myDB.cursor()
    cHandler.execute("SHOW TABLE STATUS FROM `%s`" %(username))
    results=cHandler.fetchall()
    
    if len(sys.argv) < 2:
        print "Iterate"
        for r in results:
            extractRun(r[0])
        print "Sleeping 30 min"
        time.sleep(60*30)

    else:
        run = sys.argv[1]
        extractRun(run)
        exit(1)
    
