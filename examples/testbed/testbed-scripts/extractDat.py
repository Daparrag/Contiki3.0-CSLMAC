#!/usr/bin/env python

import MySQLdb
import os
import sys
import time
import datetime
import zipfile

datfile = "14754.dat"

def extract(file):
    zfile = zipfile.ZipFile(file)
    zfile.extract(datfile, ".")
            
    runName = file.split('.')[0]
    
    outputFilePath = os.path.join("experiments", "Indriya_" + runName, "log.txt")
    outputDir = os.path.dirname(outputFilePath)
    
    if os.path.exists(outputFilePath):
        print "Indriya trace %s already converted" %(runName)    
        #return
    
    print "Converting Indriya trace %s" %(runName)
    if not os.path.exists(outputDir):
        os.makedirs(outputDir)
    
    inputFile = open(datfile, "r")
    logFile = open(outputFilePath, "w")
    baseTime = None
    
    for line in inputFile.readlines()[1:]:
        msg, insert_time, motelabMoteID, milli_time, motelabSeqNo = line.split('\t')
        motelabMoteID = int(motelabMoteID)-40000
        us_time = int(milli_time)*1000
        if baseTime == None:
            baseTime = us_time
        timestamp = us_time - baseTime
        cleanPayload = ""
        for i in range(2, len(msg), 3):
            if msg[i] == '\\' and msg[i+1] == 'n':
                cleanPayload += '\n'
                break
            else:
                cleanPayload += msg[i]
        logFile.write("%012u\tID:%u\t%s" %(timestamp, motelabMoteID, cleanPayload))
        logFile.flush()
    print "done"

extract(sys.argv[1])
