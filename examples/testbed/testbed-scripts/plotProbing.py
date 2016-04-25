#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
import matplotlib.pyplot as plt

N_NODES = 99
N_NEIGHBORS = 15.7

markers = ['s', 'o', '^', 'p', 'd']
colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', '#8FD9F2', 'black']
linestyles = ['-', '--']
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

def plotStat(all_res, field, ylabel, legendPos="lower right", legendBbox=None, legend=True):
  fig = plt.figure(figsize=(4.5, 3.5))
  ax = fig.add_subplot(111)
  x = [2, 5, 15, 30, 60]
  configs = {"nm": {'l': "Always-on", 'i': 3},
             "cm8": {"l": "ContikiMAC@8Hz", 'i': 4},
             "cm64": {"l": "ContikiMAC@64Hz", 'i': 2},
             "ts": {"l": "TSCH-minimal", 'i': 1},
             "td": {"l": "TSCH-dedicated", 'i': 0}
             }
  for mac in ["td", "ts", "cm64", "cm8", "nm"]:
    index = configs[mac]["i"]
    y = map(lambda x: all_res[(x,mac)]['stats'][field]["avg"] if (x,mac) in all_res else 0, x)
    e = map(lambda x: all_res[(x,mac)]['stats'][field]["stdev"] if (x,mac) in all_res else 0, x)
    ax.errorbar(x, y, e,
            label=configs[mac]['l'],
            linestyle=getLineStyle(index),
            marker=getMarker(index),
            color=getLineColor(index),
            linewidth=1.5,
            )
  ax.grid(True)
  if legend:
    if legendBbox != None:
      ax.legend(bbox_to_anchor=legendBbox, prop={'size':16})
    else:
      ax.legend(loc=legendPos, prop={'size':13})
  ax.set_xlabel("Packet Period (s)", fontsize=16)
  ax.set_xscale('log')
  
  if field == "dcTx":
    #ax.set_yscale('log')
    #yt = [1, 3, 10, 30]
    #ax.axis(ymin=0.1, xmin=1.7, xmax=70)
    ax.axis(xmin=1.7, xmax=70)
    #ax.set_yticks(yt)
    #ax.set_yticklabels(yt)
  elif field == "dc":
    ax.axis(ymin=0,ymax=105, xmin=1.7, xmax=70)
  else:
    ax.axis(ymin=0,xmin=1.7, xmax=70)
  
#  if field == "dc":
 #   ax.set_yscale('log')
  #  yt = [1, 3, 10, 30, 100]
   # ax.set_yticks(yt)
    #ax.set_yticklabels(yt)
  
  ax.set_xticks(x)
  ax.set_xticklabels(x)
  ax.set_ylabel(ylabel, fontsize=16)
  setp(ax.get_xticklabels(), fontsize=14)
  setp(ax.get_yticklabels(), fontsize=14)

  fig.savefig('plots/probing%s.pdf'%(field), format='pdf', bbox_inches='tight', pad_inches=0)

def extractStats(dir):
    path = os.path.join(dir, "probing.txt")
    if not os.path.isfile(path):
      return None
    f = open(path, 'r')
    last_line = f.readlines()[-1]
    res = re.compile('Overall statistics: ([\\d]+)/([\\d]+) \(([\\d.]+)\), above 90%: ([\\d]+) \(([\\d.]+)\%\), tx duty cycle ([\\d.]+)\%, duty cycle ([\\d.]+)\%').match(last_line)
    if res != None:
      return {"rxCount": float(res.group(3)), "stableLinks": int(res.group(4)),
              "dcTx": float(res.group(6)), "dc": float(res.group(7))}
    else:
      return None

def main():
  plt.rc('pdf',fonttype = 42)
  all_res = {}
  xp_dir = "experiments/probing"
  for f in os.listdir(xp_dir):
    res = re.compile('Indriya_prb([\\d]+)_([^_]+)_[\\d]+_[\\d]+').match(f)
    if res != None:
      period = int(res.group(1))
      mac = res.group(2)
      dir = os.path.join(xp_dir, f)
#      if mac == "cm8":
 #       continue
      stats = extractStats(dir)
      if stats == None:
        continue
      if not (period, mac) in all_res:
        all_res[(period, mac)] = {'data': []}
      all_res[(period, mac)]['data'].append({'dir': dir, 'stats': stats})
  for key in all_res:
    all_res[key]['stats'] = {}
    for field in ["rxCount","stableLinks","dcTx","dc"]:
      data = map(lambda x: x['stats'][field], all_res[key]['data'])
      all_res[key]['stats'][field] = {"avg": average(data),
                                      "stdev": stdev(data)}

      print key, field, all_res[key]['stats'][field]["avg"]
  plotStat(all_res, "rxCount", "Neighbors (#)")
  plotStat(all_res, "stableLinks", "Stable Links (#)",legend=False)
  plotStat(all_res, "dcTx", "Channel Utilization (%)", legendPos="upper right")
  plotStat(all_res, "dc", "Duty Cycle (%)", legendPos="upper right", legendBbox=(1,0.95),legend=False)
  
main()
