#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
from plottingTools import *
import matplotlib.pyplot as plt

barcolor = '#0a51a7'
linecolor = "none"
ecolor = '#00A876'
#colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']

N_NODES = 99
N_NEIGHBORS = 15.7

markers = ['s', 'o', '^', 'p', 'd']
colors = ['#FF5900', '#00A876', '#0a51a7', '#FF9900', '#8FD9F2', 'black']
linestyles = ['--', '-.', '-', '-']
fillColors = ['#0a51a7', '#FF9900', '#00A876', '#FF5900', '#8FD9F2', 'black']

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

def plotStat(all_res, field, filename, xlabel):
  fig = plt.figure(figsize=(4.5, 2))
  ax = fig.add_subplot(111)
  
  configs = {
             "cm8": {"l": "ContikiMAC@8Hz", 'i': 0},
             #"cm64": {"l": "ContikiMAC@64Hz", 'i': 1},
             "tmin7": {"l": "TSCH-min-7", 'i': 1},
             "trb397x31x29": {"l": "TSCH-RB-29", 'i': 2},
             "tsb397x31x29": {"l": "TSCH-SB-29", 'i': 3},
             }
  for mac in ["cm8","tmin7", "trb397x31x29", "tsb397x31x29"]:
    index = configs[mac]["i"]
    perNodeData = all_res[mac][field]['data'][0]['stats']['perNode'] # we pick the first xp
    dutyCycleList = []
    for node in perNodeData.keys():
        if node != 0:# exclude root
            dutyCycleList.append(perNodeData[node]['avg'])
    dutyCycleList = sorted(dutyCycleList)
    x = dutyCycleList
    y = arange(1,len(x)+1) * 1. / len(x)
    
    ax.plot(y, x,
            label=configs[mac]['l'],
            linestyle=getLineStyle(index),
            color=getLineColor(index),
            )
  ax.grid(True)
  
  #ax.legend(loc="lower right", prop={'size':12})
  ax.legend(loc="upper left", prop={'size':8})
  ax.set_ylabel(xlabel, fontsize=10) 
  #ax.axis(ymin=0,ymax=3.8)
  ax.tick_params(labelsize=8)
  
  #ax.set_xticks(x)
  #ax.set_xticklabels(x)
  ax.set_xlabel("CDF", fontsize=10)

  fig.savefig('plots/cdf%s.pdf'%(filename), format='pdf', bbox_inches='tight', pad_inches=0)

def getFile(base, name):
    dir = os.path.join(base, 'data')
    if os.path.isdir(dir):
        for file in os.listdir(dir):
            if name in file:
                return os.path.join(dir, file)
    return None

def extractStats(metric, dir):
    dataFile = getFile(dir, metric + '_pernode.txt')
    if dataFile != None:
        gloablData = extractGlobal(dataFile)
        perNode = extractPerNode(dataFile)
        return {'global': gloablData, 'perNode': perNode}
    return None

def main():
  plt.rc('pdf',fonttype = 42)
  all_res = {}
  xp_dir = "experiments"
  metrics = ["Duty Cycle"]
  for f in os.listdir(xp_dir):
    res = re.compile('Indriya_([^_]+)_[\\d]+_[\\d]+').match(f)
    if res != None:
      mac = res.group(1)
      dir = os.path.join(xp_dir, f)
      for m in metrics:
          stats = extractStats(m, dir)
          if stats == None:
            continue
          if not mac in all_res:
            all_res[mac] = {}
          if not m in all_res[mac]:
            all_res[mac][m] = {'data': []}
            print mac, stats['global']['perNodeMin'], stats['global']['perNodeMax']
          all_res[mac][m]['data'].append({'dir': dir, 'stats': stats})

      
  
  plotStat(all_res, "Duty Cycle", "dc", "Duty Cycle (%)")
  
main()
