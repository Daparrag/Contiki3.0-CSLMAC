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

xpNames = [
["_down_rpl1_prb1_cons1_squ1_rssi1", "Storing, all"],
["_down_rpl2_prb1_cons1_squ1_rssi1", "Non-storing, all"],
["_down_rpl1_prb0_cons0_squ0_rssi0", "Storing, none"],
["_down_rpl2_prb0_cons0_squ0_rssi0", "Non-storing, none"],
["_down_rpl1_prb0_cons0_squ1_rssi1", "Storing, aggressive"],
["_down_rpl2_prb0_cons0_squ1_rssi1", "Non-storing, aggressive"],
["_down_rpl1_prb1_cons0_squ1_rssi1", "Storing, no probing"],
["_down_rpl2_prb1_cons0_squ1_rssi1", "Non-storing, no probing"],
["_down_rpl1_prb1_cons1_squ0_rssi1", "Storing, no squetx"],
["_down_rpl2_prb1_cons1_squ0_rssi1", "Non-storing, no squetx"],
["_down_rpl1_prb1_cons1_squ1_rssi0", "Storing, no rssietx"],
["_down_rpl2_prb1_cons1_squ1_rssi0", "Non-storing, no rssietx"],
	#["_down_rpl1_prb0_cons0_squ0_rssi1", "Storing, rssi-etx"],
#["_down_rpl2_prb0_cons0_squ0_rssi1", "Non-storing, rssi-etx"],
#["_down_rpl1_prb0_cons0_squ1_rssi0", "Storing, squared-etx"],
#["_down_rpl2_prb0_cons0_squ1_rssi0", "Non-storing, squared-etx"],
#["_down_rpl1_prb1_cons0_squ0_rssi0", "Storing, probing"],
#["_down_rpl2_prb1_cons0_squ0_rssi0", "Non-storing, probing"],
#["_down_rpl1_prb1_cons1_squ0_rssi0", "Storing, probing-cons"],
#["_down_rpl2_prb1_cons1_squ0_rssi0", "Non-storing, probing-cons"],
]

metrics = ["End-to-end Delivery Ratio",
#"MAC drop", "MAC error", "No route found", "Nbr Cache Miss or Incomplete", "Not received, not dropped",
#"RPL DAO Sent", "RPL No-Path DAO Sent", "RPL MC DIO Sent", "RPL UC DIO Sent", "RPL MC DIS Sent",
#"RPL parent switch",
#"Hop Count"
]

barcolor = '#0a51a7'
linecolor = "none"
ecolor = '#00A876'
#colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']

N_NODES = 99
N_NEIGHBORS = 15.7

markers = ['s', 'o', '^', 'p', 'd']
colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']
linestyles = ['-', '--']
#fillColors = ['#0a51a7', '#FF9900', '#00A876', '#FF5900', '#8FD9F2', 'black']
fillColors = ['#0a51a7', '#FF9900']

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

def plotStat(xpdir, all_res, metric, filename, ylabel, legendPos="lower right", legendBbox=None, legend=True):
  fig = plt.figure(figsize=(4.5, 3.5))
  ax = fig.add_subplot(111)

  a = 0
  b = a+4
  c = b+2
  d = c+2
  
  w = 0.6
  xshift = 0.2
  
  x = []
  i = 0
  for xp in xpNames:
    if xp[0] in all_res:
      y = all_res[xp[0]][metric]['stats']["avg"]
      e = all_res[xp[0]][metric]['stats']["stdev"]
      if metric == "End-to-end Delivery Ratio":
        ylog = True
        if y == 100:
          y = 99.9999
        y = 1 / (1-(y / 100.))
      else:
        ylog = False
      ax.bar(i + xshift, y, w, color=getFillColor(i), ecolor=getFillColor(i), edgecolor=getFillColor(i), log=ylog)
      x.append(i)
      i += 1
  
  xindexes = map(lambda x: x[0], xpNames)
  xlabels = map(lambda x: x[1], xpNames)
  x = array(x)
  x += xshift
  y = map(lambda x: all_res[x][metric]['stats']["avg"] if x in all_res else 0, xindexes)
  e = map(lambda x: all_res[x][metric]['stats']["stdev"] if x in all_res else 0, xindexes)
    
  if metric == "End-to-end Delivery Ratio":
    #ax.axis(ymin=10, ymax=1000000)
    ax.axis(ymin=10, ymax=100000)
    #ylabels = ["9", "90", "99", "99.9", "99.99", "99.999", "99.9999"]
    ylabels = ["9", "90", "99", "99.9", "99.99", "99.999"]
    ax.set_yticklabels(ylabels)
    
  ax.yaxis.grid(True)
  ax.set_axisbelow(True) 
  ax.set_xticks(x + xshift)
  ax.set_xticklabels(xlabels, rotation=45, fontsize=14, horizontalalignment="right")
  ax.set_ylabel(ylabel, fontsize=16)
  setp(ax.get_yticklabels(), fontsize=14)
  
  plotdir = os.path.join(xpdir, 'plots')
  if not os.path.isdir(plotdir):
    os.mkdir(plotdir)
  fig.savefig(os.path.join(plotdir, 'cmp%s.pdf'%(filename)), format='pdf', bbox_inches='tight', pad_inches=0)

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
        extractGlobal
        metricInfo = extractMetricInfo(dataFile)
        gloablData = extractGlobal(dataFile)
        perNode = extractPerNode(dataFile)
        perNodeSummary = extractPerNodeSummary(dataFile)
        return {'metricInfo': metricInfo, 'global': gloablData, 'perNode': perNode, 'perNodeSummary': perNodeSummary}
    return None

def filterOutliers(data):
    #return sorted(data)[1:-1]
    return data

def main():
  plt.rc('pdf',fonttype = 42)
  all_res = {}
  xpdir = "."
  for f in os.listdir(xpdir):
    xpName = ""
    for chunk in f.split('_'):
      if not chunk.isdigit():
      	xpName += "_" + chunk

    dir = os.path.join(xpdir, f)
    for m in metrics:
        stats = extractStats(m, dir)
        if stats == None:
          continue
        if not xpName in all_res:
          all_res[xpName] = {}
        if not m in all_res[xpName]:
          all_res[xpName][m] = {'data': []}
        all_res[xpName][m]['data'].append({'dir': dir, 'stats': stats})
            
  for xp in xpNames:
    key = xp[0]
    if key in all_res:
      all_res[key]['stats'] = {}
      for m in metrics:
        data = map(lambda x: x['stats']['global']['avg'] if not x['stats']['metricInfo']['doSum'] else x['stats']['perNodeSummary']['avg'], all_res[key][m]['data'])
        data = filterOutliers(data)
        all_res[key][m]['stats'] = {"avg": average(data),
                                      "stdev": stdev(data)}
#      if m == "End-to-end Delivery Ratio":
 #         all_res[key][m]['stats']['avg'] = 100-all_res[key][m]['stats']['avg']
  #        if all_res[key][m]['stats']['avg'] == 0:
   #         all_res[key][m]['stats']['avg'] = 0.000001
        print "%40s (%3d) %40s: %10f (%10f)" %(xp[1], len(data), m, all_res[key][m]['stats']["avg"], all_res[key][m]['stats']["stdev"])
      
  plotStat(xpdir, all_res, "End-to-end Delivery Ratio", "pdr", "End-to-end PDR (%)")
  #plotStat(xpdir, all_res, "Hop Count", "hops", "Hop Count (#)")
  #plotStat(xpdir, all_res, "RPL parent switch", "switches", "Parent Switches (#)")
  
main()
