#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
from plottingTools import *
from numpy import *
import matplotlib.pyplot as plt

barcolor = '#0a51a7'
linecolor = "none"
ecolor = '#00A876'
#colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']

markers = ['s', 'o', '^', 'p', 'd']
colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']
linestyles = ['-', '--']
fillColors = ['#0a51a7', '#FF9900', '#00A876', '#FF5900', '#8FD9F2', 'black']

metrics = ["MAC Unicast Success"]
#metrics = ["Latency", "MAC Unicast Success"]

def getMarker(index):
    return markers[index % len(markers)]

def getLineStyle(index):
    return linestyles[index % len(linestyles)]
 
def getLineColor(index):
    return colors[index % len(colors)]

def getFillColor(index):
    return fillColors[index % len(fillColors)]

def stdev(data):
  avg = average(data)
  return math.sqrt(average(map(lambda x: (x - avg)**2, data)))

def plotStat(allres):
  #fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(4.5, 3.5))
  fig, (ax2) = plt.subplots(1, 1, sharex=True, figsize=(4.5, 2))
  
   
  w = 0.35
  
  for m in metrics:
      
      if m == "Latency":
          ax = ax1 
      else:
          ax = ax2
      
      perNodeDataUp = allres[m]["Up"]['perNode']
      perNodeDataDown = allres[m]["Down"]['perNode']
      
      nodes = perNodeDataUp.keys()
      x = array(nodes)
      y = map(lambda x: perNodeDataUp[x]["avg"] if x in perNodeDataUp else 0, nodes)
      y2 = map(lambda x: perNodeDataDown[x]["avg"] if x in perNodeDataDown else 0, nodes)
      e = map(lambda x: perNodeDataUp[x]["stdev"] if x in perNodeDataUp else 0, nodes)
      e2 = map(lambda x: perNodeDataDown[x]["stdev"] if x in perNodeDataDown else 0, nodes)
    
      ax.bar(x-w, y, w, color=fillColors[0], ecolor=fillColors[0], edgecolor=linecolor, label="Upwards Links")
      ax.bar(x, y2, w, color=fillColors[1], ecolor=fillColors[1], edgecolor=linecolor, label="Downwards Links")
      ax.yaxis.grid(True)
      ax.set_axisbelow(True)
         
      if m == "Latency":
          ax.axis(ymin=0)
          ax.set_ylabel("Latency (s)", fontsize=10)
      else:
          ax.axis(ymin=0, ymax=100)
          ax.set_xlabel("Node Index", fontsize=10)
          ax.set_ylabel("Link PRR (%)", fontsize=10)
      ax.tick_params(labelsize=10)
      ax.legend(bbox_to_anchor=(0., 1.02, 1., .102), loc=3,
           ncol=6, borderaxespad=0., prop={'size':8})
  
  fig.savefig('plots/iot.pdf', format='pdf', bbox_inches='tight', pad_inches=0)

def getFile(base, name):
    dir = os.path.join(base, 'data')
    if os.path.isdir(dir):
        for file in os.listdir(dir):
            if name in file:
                return os.path.join(dir, file)
    return None

def extractStats(metric, dir):
    dataFile = getFile(dir, metric + '_pernode.txt')
    print dataFile, dir, metric + '_pernode.txt'
    if dataFile != None:
        gloablData = extractGlobal(dataFile)
        print gloablData
        perNode = extractPerNode(dataFile)
        return {'global': gloablData, 'perNode': perNode}
    return None

def main():
  plt.rc('pdf',fonttype = 42)
  dir = "pitestbed-xp/315-sb29-pp/short"
  allRes = {}

  for m in metrics:
      stats_up = extractStats(m + " Up", dir)
      stats_down = extractStats(m + " Down", dir)
      allRes[m] = {"Up": stats_up, "Down": stats_down}

  plotStat(allRes)
  
main()
