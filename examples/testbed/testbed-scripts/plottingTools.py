#!/usr/bin/env python

import os
import re
import fileinput
import math
from pylab import *
import parseLogs
import pygraphviz as pgv

markers = ['s', 'o', '^', 'p', '*']
colors = ['#FF9900', '#00A876', '#0a51a7', '#FF5900', 'yellow', 'black']
linestyles = ['-', '--']
fillColors = ['#0a51a7', '#FF9900', '#00A876', '#FF5900', 'yellow', 'black']

def allStats(data):
    if data == None or data == []:
        return None
    data.sort()
    avg = average(data)
#    med = median(data)
    stdev = math.sqrt(average(map(lambda x: (x - avg)**2, data)))
    return {
            'min': min(data),
            'max': max(data), 
            'avg': avg,
#            'med': med,
            'stdev': stdev,
            'p50': percentile(data,50),
            'p80': percentile(data,80),
            'p90': percentile(data,90),
            'p95': percentile(data,95),
            'p98': percentile(data,98)
            }

def getFile(exp, base, name):
    dir = os.path.join(base, exp, 'data')
    for file in os.listdir(dir):
        if name in file:
            return os.path.join(dir, file)

def getMarker(index):
    return markers[index % len(markers)]

def getLineStyle(index):
    return linestyles[index % len(linestyles)]
 
def getLineColor(index):
    return colors[index % len(colors)]

def getFillColor(index):
    return fillColors[index % len(fillColors)]
 
def plotMetricCdf(dataSet, file, metric, legendPos=None, xlabel=None, xmin=None, xmax=None, excludeOddIds=False):
    
    fig = plt.figure(figsize=(4.5, 2.5))
    ax = fig.add_subplot(111)
            
    index = 0
    
    for config in [('rpl', 0, 'RPL'), ('orpl', 64, 'ORPL w=0.5'), ('orpl', 16, 'ORPL w=0.1')]:
        unit = dataSet[('rpl', 500, 0)]['global']['unit']
        if (config[0], 500, config[1]) in dataSet:
            metricMap = {}
            count = 0
            perNodeData = dataSet[(config[0], 500, config[1])]['iterations'][0]['perNode']
            for id in perNodeData:
                if excludeOddIds and perNodeData[id]['id'] % 2 == 1:
                    continue
                val = perNodeData[id]['avg']
                if val < 1:
                    print val, perNodeData[id]['id']
                if not val in metricMap:
                    metricMap[val] = 0
                metricMap[val] += 1
                count += 1

            x = sort(metricMap.keys())
            y = []
            sum = 0
            for i in x:
                sum += metricMap[i]/(1.*count)
                y.append(sum)
            
            ax.errorbar(x, y,
                        #yerr=e,
                        linewidth=3,
                        linestyle=getLineStyle(index),
                        color=getLineColor(index),
                        label=config[2])
            index += 1
    
    font = {'size' : 18}
    matplotlib.rc('font', **font)
    
    if legendPos != None:
        legend(loc=legendPos, prop={'size':13})
    ax.grid(True)
    ax.axis(ymin=0, ymax=1, xmin=xmin, xmax=xmax)
    if xlabel == None:
        xlabel = "%s (%s)" %(metric, unit) if unit!="" else metric
    ax.set_xlabel(xlabel, fontsize=18)
    ax.set_ylabel('CDF', fontsize=18)
    
    fig.savefig('plots/%s.pdf'%(file), format='pdf', bbox_inches='tight', pad_inches=0)
 
def calculatePercentilePerNode(perNodeMap, percentile):

    data = map(lambda x: x[1]['avg'], perNodeMap.items())
    
    data.sort()

    index = percentile*(len(data)-1)/100.

    if index == int(index):
        return data[int(index)]
    else:
        f1 = index-int(index)
        f2 = 1 - f1
        v1 = data[int(index)]
        v2 = data[int(index)+1]
        return (v1+v2)/2.
 
def plotMetricVsBloomM(dataSet, file, metric, legendPos=None, ymin=0, ymax=None, ylabel=None):
    
    fig = plt.figure(figsize=(3.8, 3.5))
    ax = fig.add_subplot(111)
    
   
    unit = dataSet[(512, 4, 1)]['global']['unit']
    
    configs = [(64, 1, 1), (128, 2, 1), (256, 4, 1), (384, 4, 1), (512, 4, 1), (640, 6, 1), (512, 0, 0)];
    x = map(lambda x: x[0] if x[1]!=0 else 768, configs)
    xlabels = map(lambda x: x[0]/8 if x[1]!=0 else "bm", configs)
        
    index = 0
    for percentile in [95, 80, 50]:

        y = map(lambda x: calculatePercentilePerNode(dataSet[x]['iterations'][0]['perNode'], percentile) if (x) in dataSet else 0, configs)
#        y = map(lambda x: dataSet[x]['percentiles']['p%d'%percentile] if (x) in dataSet else None, configs)
        ax.errorbar(x, y,
                    #yerr=e,
                    linewidth=2,
                    linestyle=getLineStyle(2-index),
                    marker=getMarker(2-index),
                    color=getLineColor(2-index),
                    markersize=8,
                    label="%uth percentile"%(percentile)
                    )
        index += 1
    
    font = {'size' : 14}
    matplotlib.rc('font', **font)
    
    config = (512, 4, 1)
    print str(config) + " avg %.2f, avgStdevPerNode %.2f, maxStdevPerNode %.2f" %(dataSet[config]['global']['avg'], dataSet[config]['allPerNodeStdev']['avg'], dataSet[config]['allPerNodeStdev']['max'])
    
    if legendPos != None:
        legend(loc=legendPos, prop={'size':14})
    ax.grid(True)
    ax.set_xticks(x)
    ax.set_xticklabels(xlabels)
    ax.axis(ymin=ymin, ymax=ymax)
    if ylabel == None:
        ylabel = "%s (%s)" %(metric, unit) if unit!="" else metric
    ax.set_xlabel('Bloom Filter Size, m (bytes)', fontsize=16)
    ax.set_ylabel(ylabel, fontsize=16)
    
    fig.savefig('plots/%s.pdf'%(file), format='pdf', bbox_inches='tight', pad_inches=0)
 
def plotMetricVsBloomK(dataSet, file, metric, legendPos=None, ymin=0, ymax=None, ylabel=None):
    
    fig = plt.figure(figsize=(3.8, 3.5))
    ax = fig.add_subplot(111)
    
    index = 0
    x = []
    for key in dataSet.keys():
        if key[0] == 256:
            k = key[1]
            if not k in x:
                x.append(k)
    x.sort()

    unit = dataSet[(256, 2, 1)]['global']['unit']
        
    index = 0
    for percentile in [95, 80, 50]:
        y = map(lambda x: calculatePercentilePerNode(dataSet[(256, x, 1)]['iterations'][0]['perNode'], percentile) if (256, x, 1) in dataSet else None, x)
        #y = map(lambda x: dataSet[x]['percentiles']['p%d'%percentile] if (x) in dataSet else None, configs)
        
        #e = map(lambda x: dataSet[(x, kmap[x], 1)]['global']['perNodeStdev'] if (x, kmap[x], 1) in dataSet else None, x)
        ax.errorbar(x, y,
                    #yerr=e,
                    linewidth=2,
                    linestyle=getLineStyle(2-index),
                    marker=getMarker(2-index),
                    color=getLineColor(2-index),
                    markersize=8,
                    label="%uth percentile"%(percentile)
                    )
        index += 1
    
    font = {'size' : 14}
    matplotlib.rc('font', **font)
    
    if legendPos != None:
        legend(loc=legendPos, prop={'size':12})
    ax.grid(True)
    
    ax.set_xticks(x)
    ax.set_xticklabels(x)
    ax.axis(ymin=ymin, ymax=ymax)
    if ylabel == None:
        ylabel = "%s (%s)" %(metric, unit) if unit!="" else metric
    ax.set_xlabel('Number of Hashes, k', fontsize=16)
    ax.set_ylabel(ylabel, fontsize=16)
    
    fig.savefig('plots/%s.pdf'%(file), format='pdf', bbox_inches='tight', pad_inches=0)
 
def smooth_over(l,c):
    smoothed = list(l)
    for i in range(c,len(l)-c):
        sum = 0.
        for j in range(i-c,i+1):
            sum += l[j]
        smoothed[i] = sum/(c+1)
    return smoothed
 
def plotTimeline(ax, dataSet, file, metric, legendPos=None, ymin=0, ymax=None, xlabel=None, ylabel=None, smooth_level=0, downsample=1):
        
    index = 0
    
    for t in range(50, 120, 25):
        ax.fill([t-10, t-10, t, t], [ymin, ymax, ymax, ymin], linewidth=0, color='#FFD073')

#colors = ['#0a51a7', '#FF9900', '#', '#FF5900', 'yellow', 'black']
    #for config in [('orpl', 'ORPL', '#0a51a7', '-'), ('orpl-nofp', 'ORPL (no recovery)', '#0a51a7', ':'), ('rpl', 'RPL', '#67e667', '-')]:
    for config in [('orpl', 1, 1, 'ORPL (Bloom filter)', '#0a51a7', '-'), ('orpl', 0, 0, 'ORPL (Bitmap)', '#0a51a7', ':'), ('rpl', 0, 0, 'RPL', '#67e667', '-')]:
        protocol = config[0]
        bf = config[1]
        fpr = config[2]
        color = config[4]
        linestyle = config[5]
        conf = (protocol, bf, fpr)
        if conf in dataSet:
            x = dataSet[conf]['timeline'].keys()
            x.sort()
            timeline = dataSet[conf]['timeline']
            y = map(lambda x: timeline[x]['avg'] if (conf) in dataSet else None, x)
            ysmoothed = smooth_over(y,smooth_level)
            
            ax.errorbar(x[0::downsample], ysmoothed[0::downsample],
                        linewidth=1.8,
                        linestyle = linestyle,
                        marker=None,
                        color=color,
                        label=config[3])
        index += 1
    
    handles, labels = ax.get_legend_handles_labels()
    handles = handles + [Rectangle((0, 0), 1, 1, fc="#FFD073", linewidth=0)]
    labels = labels + ['Outage']

    font = {'size' : 14}
    matplotlib.rc('font', **font)

    ax.locator_params(nbins=5, axis='y')

    if legendPos != None:
        ax.legend(handles, labels, loc="upper center", prop={'size':12}, ncol=4,  bbox_to_anchor=(0.475, 1.35))
    ax.grid(True)
    if xlabel != None:
        ax.set_xticks([10, 30, 50, 70, 90, 110])
        ax.set_xticklabels([0, 20, 40, 60, 80, 100])
        ax.set_xlabel(xlabel, fontsize=14)
    if metric == 'PRR':
        ax.set_yticks([0,20,40,60,80,100])
            
    ax.minorticks_off()
    #ax.xaxis.set_major_formatter(ScalarFormatter())
    ax.axis(ymin=ymin, ymax=ymax, xmin=10, xmax=110)
    if ylabel == None:
        ylabel = "%s (%s)" %(metric, unit) if unit!="" else metric
    ax.set_ylabel(ylabel, fontsize=13, multialignment='center')
    
    #fig.savefig('plots/%s.pdf'%(file), format='pdf', bbox_inches='tight', pad_inches=0)
 
def plotMetricVsCycleTime(dataSet, file, metric, legendPos=None, ymin=0, ymax=None, ylabel=None, invertLegend=False):
        
    fig = plt.figure(figsize=(4.5, 3.5))
    ax = fig.add_subplot(111)
    
    index = 0
    x = []
    for key in dataSet.keys():
        ct = key[1]
        if not ct in x:
            x.append(ct)
    x.sort()
#    for config in [('rpl', 0, 'RPL'), ('orpl', 16, 'ORPL w=0.1'), ('orpl', 64, 'ORPL w=0.5')]:

    configs = [('orpl', 64, 'ORPL w=0.5'), ('orpl', 16, 'ORPL w=0.1'), ('rpl', 0, 'RPL')]

    for config in configs:
        unit = dataSet[('orpl', 500, 64)]['global']['unit']
        y = map(lambda x: dataSet[(config[0], x, config[1])]['global']['avg'] if (config[0], x, config[1]) in dataSet else 0, x)
        e = map(lambda x: dataSet[(config[0], x, config[1])]['global']['stdev'] if (config[0], x, config[1]) in dataSet else 0, x)
        ax.errorbar(x, y,
                    linewidth=2,
                    yerr=e,
                    linestyle=getLineStyle(index),
                    marker=getMarker(index),
                    color=getLineColor(index),
                    markersize=8,
                    label=config[2])
        index += 1
        for i in range(len(x)):
            if (config[0], x[i], config[1]) in dataSet:
                if x[i] == 500:
                    print str(config) + " ct %d: avg %.2f, avgStdevPerNode %.2f, maxStdevPerNode %.2f" %(x[i], y[i], dataSet[(config[0], x[i], config[1])]['allPerNodeStdev']['avg'], dataSet[(config[0], x[i], config[1])]['allPerNodeStdev']['max'])
                
    font = {'size' : 14}
    matplotlib.rc('font', **font)
    if ylabel == None:
        ylabel = "%s (%s)" %(metric, unit) if unit!="" else metric
                
    if legendPos != None:
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles[::-1], labels[::-1], loc=legendPos, prop={'size':16})
    ax.grid(True)
    ax.set_xlabel('Wakeup Interval (ms)', fontsize=16)
    ax.set_ylabel(ylabel, fontsize=16)
    ax.set_xscale('log', basex=2)
    ax.set_xticks(x)
    ax.minorticks_off()
    ax.xaxis.set_major_formatter(ScalarFormatter())
    ax.axis(ymin=ymin, ymax=ymax, xmin=100, xmax=x[-1]*(128/100.))
    
    fig.savefig('plots/%s.pdf'%(file), format='pdf', bbox_inches='tight', pad_inches=0)

def extractAnyToAnyStats(file):
    f = open(file, 'r')
    dataSet = {}
    print file
    for line in f.readlines():
        res = re.compile('(..) \((\\d*)\) -> (..) \((\\d*)\): rrprr ([\\d.]*), rrlatency mean ([\\d.]*), rrlatency stdev ([\\d.]*), hops ([\\d.]*), hops stdev ([\\d.]*)').match(line)
        #res = re.compile('(.*?) (\\d*) -> (.*?) (\\d*)').match(line)
        #res = re.compile('(..)').match(line)
        if res != None:
            srcPos = res.group(1)
            srcId = int(res.group(2))
            dstPos = res.group(3)
            dstId = int(res.group(4))
            prr = float(res.group(5))
            latencyAvg = float(res.group(6))
            latencyStdev = float(res.group(7))
            hopsAvg = float(res.group(8))
            hopsStdev = float(res.group(9))
            stats = {'prr': prr, 'latencyAvg': latencyAvg, 'latencyStdev': latencyStdev, 'hopsAvg': hopsAvg, 'hopsStdev': hopsStdev}
            dataSet[(srcPos, dstPos)] = stats
    return dataSet
            
def extractMetricBloom(metric):

    dataSet = {}
    baseDir = 'experiments/bloom'
    for d in os.listdir(baseDir):
        res = re.compile('iteration(\d*)').match(d)
        if res:
            iteration = int(res.group(1))
            iterationDir = os.path.join(baseDir, d)
            for d in os.listdir(iterationDir):
                res = re.compile('Indriya_orpl_down_m(\d*)_k(\d*)_fpr(\d*)\d*_\d*').match(d)
                if res:
                    m = int(res.group(1))
                    k = int(res.group(2))
                    fpr = int(res.group(3))  
                    dataFile = getFile(d, iterationDir, metric + '_pernode.txt')
                    if dataFile != None:
                        gloablData = extractGlobal(dataFile)
                        perNode = extractPerNode(dataFile)
                        if not ((m, k, fpr)) in dataSet:
                            dataSet[((m, k, fpr))] = {'iterations': []}
                        dataSet[((m, k, fpr))]['iterations'].append({'global': gloablData, 'perNode': perNode})
    
    for config in dataSet:
        allAvg = map(lambda x: x['global']['avg'], dataSet[config]['iterations'])
        allStdev = map(lambda x: x['global']['stdev'], dataSet[config]['iterations'])
        allPerNodeStdev = map(lambda x: x['global']['perNodeStdev'], dataSet[config]['iterations'])
        dataSet[config]['allAvg'] = allStats(allAvg)  
        dataSet[config]['allStdev'] = allStats(allStdev)
        dataSet[config]['allPerNodeStdev'] = allStats(allPerNodeStdev)
        dataSet[config]['global'] = allStats(allAvg)
        dataSet[config]['global']['unit'] = dataSet[config]['iterations'][0]['global']['unit']

    return dataSet
        
def extractAnyToAny():
    dataSet = {}
    baseDir = 'experiments/anytoany/iteration1'
    for exp in os.listdir(baseDir):
        res = re.compile('Indriya_(.*?)_anytoany_ct(\d*)_w(\d*)_\d*_\d*').match(exp)
        print res
        if res:
            protocol = res.group(1)
            ct = int(res.group(2))
            w = int(res.group(3)) 
            statFile = os.path.join(baseDir, exp, 'anyToAnyStats.txt')
            stats = extractAnyToAnyStats(statFile)
            dataSet[(protocol, ct, w)] = stats

    return dataSet
    
def extractTimelineOutage(metric):
    dataSet = {}
    for exp in os.listdir('experiments/outage'):
        res = re.compile('Indriya_(.*?)_down_outage_bf(.*?)_fpr(.*?)_\d*_\d*').match(exp)
        if res:
            protocol = res.group(1)
            bf = int(res.group(2))
            fpr = int(res.group(3))
            prrFile = getFile(exp, 'experiments/outage', metric + '_timeline.txt')
            if prrFile != None:
                prrData = extractGlobal(prrFile)
                timeline = extractTimeline(prrFile)
                dataSet[(protocol, bf, fpr)] = {'global': prrData, 'timeline': timeline}

    return dataSet
    
def extractMetricGeneric(metric, xpname, baseDir):
    dataSet = {}
    for d in os.listdir(baseDir):
        res = re.compile('iteration(\d*)').match(d)
        if res:
            iteration = int(res.group(1))
            iterationDir = os.path.join(baseDir, d)
            for d in os.listdir(iterationDir):
                res = re.compile('Indriya_(.*?)_%s_ct(\d*)_w(\d*)_*' %(xpname)).match(d)
                if res:
                    protocol = res.group(1)
                    ct = int(res.group(2))
                    w = int(res.group(3)) 
                    key = (protocol, ct, w)
                    print key
                else:
                    res = re.compile('Indriya_olwb_ll_small_*').match(d)
                    if res:
                        key = 'lwb-small'
                    else:
                        res = re.compile('Indriya_olwb_ll_*').match(d)
                        if res:
                            key = 'lwb'
                if key != None:
                    dataFile = getFile(d, iterationDir, metric + '_pernode.txt')
                    if dataFile != None:
                        gloablData = extractGlobal(dataFile)
                        perNode = extractPerNode(dataFile)
                        if not key in dataSet:
                            dataSet[key] = {'iterations': []}
                        dataSet[key]['iterations'].append({'global': gloablData, 'perNode': perNode})
    
    for config in dataSet:
        allAvg = map(lambda x: x['global']['avg'], dataSet[config]['iterations'])
        allStdev = map(lambda x: x['global']['stdev'], dataSet[config]['iterations'])
        allPerNodeStdev = map(lambda x: x['global']['perNodeStdev'], dataSet[config]['iterations'])
        allPerNodeMin = map(lambda x: x['global']['perNodeMin'], dataSet[config]['iterations'])
        allPerNodeMax = map(lambda x: x['global']['perNodeMax'], dataSet[config]['iterations'])
        dataSet[config]['allAvg'] = allStats(allAvg)  
        dataSet[config]['allStdev'] = allStats(allStdev)
        dataSet[config]['allPerNodeStdev'] = allStats(allPerNodeStdev)
        dataSet[config]['allPerNodeMin'] = allStats(allPerNodeMin)
        dataSet[config]['allPerNodeMax'] = allStats(allPerNodeMax)
        dataSet[config]['global'] = allStats(allAvg)
        dataSet[config]['global']['unit'] = dataSet[config]['iterations'][0]['global']['unit']

    return dataSet

def extractMetric(metric):
    return extractMetricGeneric(metric, 'collect', 'experiments/')

def extractMetricCollect(metric):
    return extractMetricGeneric(metric, 'collect', 'experiments/up-traffic')

def extractMetricDown(metric):
    return extractMetricGeneric(metric, 'down', 'experiments/down-traffic')

def extractMetricAnyToAny(metric):
    return extractMetricGeneric(metric, 'anytoany', 'experiments/anytoany')    

def extractMetricInfo(file):
    f = open(file, 'r')
    for line in f.readlines():
        if line.startswith('#'):
            res = re.compile('.*doSum ([\\d.-]+), doMax ([\\d.-]+)').match(line)
            if res != None:
                info = {}
                info['doSum'] = int(res.group(1)) == 1
                info['doMax'] = int(res.group(2)) == 1
                return info

def extractGlobal(file):
    f = open(file, 'r')
    for line in f.readlines():
        if line.startswith('#'):
            res = re.compile('\#\\sGlobal:\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s(.*)').match(line)
            if res != None:
                globalData = {}
                globalData['avg'] = float(res.group(1))
                globalData['min'] = float(res.group(2))
                globalData['max'] = float(res.group(3))
                globalData['stdev'] = float(res.group(4))
                globalData['perNodeStdev'] = float(res.group(5))
                globalData['perNodeMin'] = float(res.group(6))
                globalData['perNodeMax'] = float(res.group(7))
                globalData['unit'] = res.group(8)
                return globalData

def extractPercentiles(file):
    f = open(file, 'r')
    for line in f.readlines():
        if line.startswith('#'):
            res = re.compile('\#\\sPercentiles:\\s50=([\\d.-]+)\\s80=([\\d.-]+)\\s90=([\\d.-]+)\\s95=([\\d.-]+)\\s98=([\\d.-]+)').match(line)
            if res != None:
                globalData = {}
                globalData['p50'] = float(res.group(1))
                globalData['p80'] = float(res.group(2))
                globalData['p90'] = float(res.group(3))
                globalData['p95'] = float(res.group(4))
                globalData['p98'] = float(res.group(5))
                return globalData

def extractPerNodeSummary(file):
    f = open(file, 'r')
    for line in f.readlines():
        if line.startswith('#'):
            res = re.compile('\#\\sPerNode Summary:\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)').match(line)
            if res != None:
                perNodeSummary = {}
                perNodeSummary['avg'] = float(res.group(1))
                perNodeSummary['min'] = float(res.group(2))
                perNodeSummary['max'] = float(res.group(3))
                perNodeSummary['stdev'] = float(res.group(4))
                return perNodeSummary

def extractPerNode(file):
    perNodeData = {}
    f = open(file, 'r')
    for line in f.readlines():
        res = re.compile('([\\d]+)\\s([\\d]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)').match(line)
        if res != None:
            index = int(res.group(1))
            perNodeData[index] = {}
            perNodeData[index]['id'] = int(res.group(2))
            perNodeData[index]['avg'] = float(res.group(3))
            perNodeData[index]['min'] = float(res.group(4))
            perNodeData[index]['max'] = float(res.group(5))
            perNodeData[index]['stdev'] = float(res.group(6))
    return perNodeData

def extractTimeline(file):
    timelineData = {}
    f = open(file, 'r')
    for line in f.readlines():
        res = re.compile('([\\d.]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)\\s([\\d.-]+)').match(line)
        if res != None:
            t = float(res.group(1))
            timelineData[t] = {}
            timelineData[t]['avg'] = float(res.group(2))
            timelineData[t]['min'] = float(res.group(3))
            timelineData[t]['max'] = float(res.group(4))
            timelineData[t]['stdev'] = float(res.group(5))
    
    return timelineData
