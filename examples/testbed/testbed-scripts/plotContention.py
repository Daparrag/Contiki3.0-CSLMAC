#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv
import matplotlib.pyplot as plt

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

def macName(baseName, x):
    if x == 100:
        if baseName == "tsb":
            return "tsb397x31x47x53"
        else:
            return "trb397x31x101"
    else:
        return "%s397x31x%d" %(baseName,x)

def plotStat(all_res, field, ylabel):
  fig = plt.figure(figsize=(4.5, 3.5))
  ax = fig.add_subplot(111)
  x_all = [3, 7, 17, 29, 47, 100]
  x_all_labels = [3, 7, 17, 29, 47, "47+53"]
  configs = {"tsb": {'l': "TSCH-SB", 'i': 0},
             "trb": {"l": "TSCH-RB", 'i': 1},
             }
  for mac in ["tsb", "trb"]:
    index = configs[mac]["i"]
    x = filter(lambda x: macName(mac, x) in all_res, x_all)
    y = map(lambda x: all_res[macName(mac, x)]['stats'][field]["avg"], x)
    e = map(lambda x: all_res[macName(mac, x)]['stats'][field]["stdev"], x)
    ax.errorbar(x, y, e,
            label=configs[mac]['l'],
            linestyle=getLineStyle(index),
            marker=getMarker(index),
            color=getLineColor(index),
            linewidth=1.5,
            )
  ax.grid(True)
  
  if field == "prr":
      #ax.legend(loc="lower left", prop={'size':12})
      ax.axis(xmin=3, ymin=75, ymax=100)
  else:
      ax.legend(loc="upper left", prop={'size':16})
      ax.axis(xmin=3, ymin=0)
  ax.set_xlabel("Unicast Slotframe Len (slots)", fontsize=18)
  ax.set_xscale('log')
  
  #ax.axis(ymin=0, xmin=1.7, xmax=70)
  
  
#  if field == "dc":
 #   ax.set_yscale('log')
  #  yt = [1, 3, 10, 30, 100]
   # ax.set_yticks(yt)
    #ax.set_yticklabels(yt)
  
  ax.set_xticks(x_all)
  ax.set_xticklabels(x_all_labels)
  ax.set_ylabel(ylabel, fontsize=18)
  setp(ax.get_xticklabels(), fontsize=16)
  setp(ax.get_yticklabels(), fontsize=16)

  fig.savefig('plots/cont%s.pdf'%(field), format='pdf', bbox_inches='tight', pad_inches=0)

def extractStats(dir):
    path = os.path.join(dir, "contentionlog.txt")
    if not os.path.isfile(path):
      return None
    f = open(path, 'r')
    lines = f.readlines()
    first_line = lines[0]
    last_line = lines[-1]
        
    res = re.compile('Portion of attempted Rx having contenders: ([\d]+)/([\d]+)').match(last_line)
    if res != None:
      contendedAttempts = int(res.group(1))
      totalAttempts = int(res.group(2))
      res = re.compile('Overall Rx statistics: ([\d]+)/([\d]+)').match(first_line)
      if res != None:
          totalSuccess = int(res.group(1))      
          return {"contendedAttempts": contendedAttempts, "totalAttempts": totalAttempts, "totalSuccess": totalSuccess,
                  "contentionPercent": 100.* contendedAttempts / totalAttempts, "prr": 100.*totalSuccess/totalAttempts
                  }
    else:
      return None

def main():
  plt.rc('pdf',fonttype = 42)
  all_res = {}
  xp_dir = "experiments"
  for f in os.listdir(xp_dir):
    res = re.compile('Indriya_([^_]+)_[\\d]+_[\\d]+').match(f)
    if res != None:
      mac = res.group(1)
      dir = os.path.join(xp_dir, f)
      stats = extractStats(dir)
      if stats == None:
        continue
      if not mac in all_res:
        all_res[mac] = {'data': []}
      all_res[mac]['data'].append({'dir': dir, 'stats': stats})
  for key in all_res:
    all_res[key]['stats'] = {}
    for field in ["contentionPercent", "prr"]:
      data = map(lambda x: x['stats'][field], all_res[key]['data'])
      all_res[key]['stats'][field] = {"avg": average(data),
                                      "stdev": stdev(data)}

      print key, field, all_res[key]['stats'][field]["avg"]
  plotStat(all_res, "contentionPercent", "Contention Rate (%)")
  plotStat(all_res, "prr", "Link PRR (%)")
    
main()
