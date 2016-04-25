#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
import plottingTools

MIN_TIME = 0 # start counting only after 10 minutes (except for timeline)
MAX_TIME = 0
SINK_ID = 2

# aggregation functions
def plotTimeline(plottableData, standAlone=False, xLabel=True):
    unitstr = "" if plottableData['global']['unit'] == "" else " (%s)"%(plottableData['global']['unit'])
    x = plottableData['timeline'].keys()
    x.sort()
    
    y = map(lambda x: plottableData['timeline'][x]['avg'], x)
    if plottableData['global'] != None:
        yavg = map(lambda x: plottableData['global']['avg'], x)
    e = map(lambda x: plottableData['timeline'][x]['stdev'], x)

    if xLabel:
        xlabel('time (min)')
    
    ylabel(plottableData['name'] + unitstr)
                
    if x != []:
        errorbar(x, y, yerr=e, fmt='bo', color='#3366CC')
    plot(x, y, color='#3366CC')
    if plottableData['global'] != None:
        plot(x, yavg, color='#FF9900')

    plt.axis(xmin=MIN_TIME)
    plt.axis(xmax=MAX_TIME)

    if plottableData["global"]["perNodeMin"] == plottableData["global"]["perNodeMax"]:
        plt.axis(ymin=plottableData["global"]["perNodeMin"]-1)
        plt.axis(ymax=plottableData["global"]["perNodeMax"]+1)
    else:
        plt.axis(ymin=0)

def plotPerNode(plottableData, standAlone=False, xLabel=True):
    unitstr = "" if plottableData['global']['unit'] == "" else " (%s)"%(plottableData['global']['unit'])
    x = plottableData['perNode'].keys()
    x.sort()
    
    y = map(lambda x: plottableData['perNode'][x]['avg'], x)
    if plottableData['global'] != None:
        yavg = map(lambda x: plottableData['global']['avg'], x)
    
    if xLabel:
        xlabel('node index')
    ylabel(plottableData['name'] + unitstr)
    bar(x, y, color='#3366CC', edgecolor='none')

    highlight = -1
    y = map(lambda x: plottableData['perNode'][x]['avg'] if plottableData['perNode'][x]['id'] == highlight else 0, x)
    bar(x, y, color='r')
    
    if plottableData['global'] != None:
        plot(x, yavg, color='#FF9900')
    
    plt.axis(xmax=len(x))
    if plottableData["global"]["perNodeMin"] == plottableData["global"]["perNodeMax"]:
        plt.axis(ymin=plottableData["global"]["perNodeMin"]-1)
        plt.axis(ymax=plottableData["global"]["perNodeMax"]+1)
    else:
        plt.axis(ymin=0)
    
def main():
    global MAX_TIME
    
    allPlottableData = []
    
    if len(sys.argv) < 2:
        dirName = '.'
    else:
        dirName = sys.argv[1].rstrip('/')

    experiment = os.path.basename(os.path.abspath(dirName))
    dir = os.path.join(dirName, 'data')

    for file in os.listdir(dir):
        if file.endswith('pernode.txt'):
            baseName = file[0:file.rfind('_')]
            allPlottableData.append({
                'name': baseName[file.find('_')+1:],
                'global': plottingTools.extractGlobal(os.path.join(dir,baseName+'_pernode.txt')),
                'perNode': plottingTools.extractPerNode(os.path.join(dir,baseName+'_pernode.txt')),
                'timeline': plottingTools.extractTimeline(os.path.join(dir,baseName+'_timeline.txt'))
            })
            
    print "\nGenerating plots:"

    rows = (len(allPlottableData))
    fig = plt.figure(figsize=(7*2,4*rows))
    #fig.suptitle(experiment)
       
    index = 1
    for plottableData in allPlottableData:
        print plottableData['name']

        if len(plottableData['timeline'].keys()) > 0:
            MAX_TIME = max(MAX_TIME, plottableData['timeline'].keys()[-1])
        
        if plottableData['global']['min'] == plottableData['global']['max'] == 0:
            continue

        plt.subplot(rows,2,index)
        plotTimeline(plottableData)

        index += 1
        
        plt.subplot(rows,2,index)
        plotPerNode(plottableData)    
        index += 1
    
    destDir = os.path.join(dirName, 'plots')
    destFile = os.path.join(destDir, "allplots")
    if not os.path.exists(destDir):
        os.makedirs(destDir)
    fig.savefig('%s.pdf' %(destFile), format='pdf', bbox_inches='tight')
                
    print "Done."
    
main()
