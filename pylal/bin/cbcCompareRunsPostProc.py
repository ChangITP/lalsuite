#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#       cbcCompareRunsPostProc.py
#
#       Copyright 2011
#       Salvatore Vitale <salvatore.vitale@ligo.org>
#
#
#       This program is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 2 of the License, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program; if not, write to the Free Software
#       Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#       MA 02110-1301, USA.

#===============================================================================
# Preamble
#===============================================================================

#standard library imports
import sys
import os

from math import ceil,floor,pi as pi_constant
import cPickle as pickle

from time import strftime

#related third party imports

import numpy as np
from numpy import array,exp,cos,sin,arcsin,arccos,sqrt,size,mean,column_stack,cov,unique,hsplit,correlate,log,dot,power
from scipy import *
from scipy import stats as stat
import matplotlib 
matplotlib.use("Agg")
from pylab import *
from  matplotlib import pyplot as plt,cm as mpl_cm
from mpl_toolkits.basemap import Basemap
try:
    from xml.etree.cElementTree import Element, SubElement, ElementTree, Comment, tostring, XMLParser,XML
except ImportError:
    #Python < 2.5
    from cElementTree import Element, SubElement, ElementTree, Comment, tostring, XMLParser,XML

#local application/library specific imports
from pylal import SimInspiralUtils
from pylal import bayespputils as bppu
from pylal import git_version

__author__="Salvatore Vitale <salvatore.vitale@ligo.org>"
__version__= "git id %s"%git_version.id
__date__= git_version.date

def pickle_to_file(obj,fname):
    """
    Pickle/serialize 'obj' into 'fname'.
    """
    filed=open(fname,'w')
    pickle.dump(obj,filed)
    filed.close()

def oneD_dict_to_file(dict,fname):
    filed=open(fname,'w')
    for key,value in dict.items():
        filed.write("%s %s\n"%(str(key),str(value)) )
        
def remove_ifo_from_string(name):
    ### Take a string like "H1: 10.0", and return a list whose first element is the IFO name and the second a string containing the remaining of "name", excluding the period 
    ifos=['H1','L1','V1','Network']
    for ifo in ifos:
        if name.find(ifo)!=-1:
            return [name[0:len(ifo)],str(name[len(ifo)+1:])]

def linear_space( start, end, count ):
    delta = (end-start) / float(count)
    return [start,] + \
    map( lambda x:delta*x + start, range( 1, count ) ) + [end, ]
     
def linkImage(imageurl,width,height):
        return '<a href="'+ str(imageurl)+'" target="_blank"><img src="'+str(imageurl)+'" width="'+str(width) +'" height="'+str(height)+'" /></a>'
def link(url,text):
        return '<a href="'+ str(url)+'">'+str(text)+'</a>'
def checkDir(dir):
        if not os.access(dir,os.F_OK):
                try:
                        print 'Creating %s\n'%(dir)
                        os.makedirs(dir)
                except:
                        print 'Error: Unable to create directory %s'%(dir)
                        sys.exit(1)
        if not os.access(dir,os.W_OK):
                print 'Unable to write to %s'%(dir)
                sys.exit(1)
        return True
        
def std_dev(array,mean):
    return sqrt(sum((x-mean) ** 2 for x in array) / len(array))
def mean(array):
    return np.mean(array)
def skewness(array,mean,std_dev):
    return (sum((x-mean) ** 3 for x in array) / (len(array)*std_dev **3) )
def kurtosis(array,mean,std_dev):
    return (sum((x-mean) ** 4 for x in array) / (len(array)*std_dev **4) )
def read_snr(snrs,run,time,IFOs):
    ### Check if files containing SNRs for sigle IFO are present in the folder snrs[run]. If not, check whether a file containing all the SNRs is present in that folder. In case of success return a list whose first element is a string containing the SNRs values and the second element a list of strings each containing the IFO names with the prefix "SNR_"
    snr_values=""
    snr_header=[]
    net_snr2=0.0
    for IFO in IFOs:
        path_to_file=os.path.join(snrs[run],'snr_'+IFO+'_'+str(time)+'.0.dat')
        path_to_multifile=os.path.join(snrs[run],'snr_'+''.join(IFO for IFO in IFOs)+'_'+str(time)+'.0.dat')
        if os.path.isfile(path_to_file):
            snr_file=open(path_to_file,'r')
            line=snr_file.readline()[:-1]
            snr_header.append('SNR_'+remove_ifo_from_string(line)[0])
            snr_values+=str(remove_ifo_from_string(line)[1])+' '
            net_snr2+=remove_ifo_from_string(line)[1]**2
            snr_file.close()
    if snr_header!=[]:
        snr_header.append('SNR_Network')
        snr_values+=str(sqrt(net_snr2))
    if os.path.isfile(path_to_multifile) and snr_header==[]:
        snr_file=open(path_to_multifile,'r')
        for line in snr_file.readlines()[:-1]:
            snr_header.append('SNR_'+remove_ifo_from_string(line)[0])
            snr_values +=remove_ifo_from_string(line)[1][:-1]
        snr_file.close()
    return [snr_values,snr_header]

def Make_injected_sky_map(dec_ra_inj,outdir,run,dec_ra_cal=None,dec_ra_ctrl=None):
    """
    Plots a sky map using the Mollweide projection in the Basemap package. 
    The map contains the injected points, the recovered points with the control run (if given) annd the recovered points with the cal run (if given)

    @dec_ra_inj is a list of the kind [[dec1,ra1],[dec2,ra2],[etc]] where (decN,raN) are the coordinates of the Nth injected point.
    @dec_ra_cal is a list of the correnspoding coordinates as recovered in the runs affected by calibration errors
    @dec_ra_ctrl is a list of the corrensponding coordinates as recovered in the control runs
    @param outdir: Output directory in which to save skymap.png image.
    """
    from matplotlib.path import Path
        
    path_plots=os.path.join(outdir,run,'SkyPlots')
    checkDir(path_plots)
    np.seterr(under='ignore')
    m=Basemap(projection='moll',lon_0=180.0,lat_0=0.0,anchor='W')
    
    if dec_ra_inj is not None:
        dec_ra_inj=(np.asarray(dec_ra_inj))[:,0:-1]
        ra_reverse = 2*pi_constant - np.asarray(dec_ra_inj)[::-1,1]*57.296
        plx,ply=m(
                  ra_reverse,
                  np.asarray(dec_ra_inj)[::-1,0]*57.296
                  )
    if dec_ra_ctrl is not None:
        bsn_ctrl=np.asarray(dec_ra_ctrl)[:,-1][::-1]
        dec_ra_ctrl=np.asarray(dec_ra_ctrl)[:,0:-1]
        ra_reverse_ctrl = 2*pi_constant - np.asarray(dec_ra_ctrl)[::-1,1]*57.296
        plx_ctrl,ply_ctrl=m(
                ra_reverse_ctrl,
                np.asarray(dec_ra_ctrl)[::-1,0]*57.296
                )
        if dec_ra_inj is not None:
            vert_x_ctrl=column_stack((plx_ctrl,plx))
            vert_y_ctrl=column_stack((ply_ctrl,ply))            
    if dec_ra_cal is not None:
        bsn_cal=np.asarray(dec_ra_cal)[:,-1][::-1]
        dec_ra_cal=np.asarray(dec_ra_cal)[:,0:-1]
        ra_reverse_cal = 2*pi_constant - np.asarray(dec_ra_cal)[::-1,1]*57.296
        plx_cal,ply_cal=m(
                  ra_reverse_cal,
                  np.asarray(dec_ra_cal)[::-1,0]*57.296
                  )
        if dec_ra_inj is not None:
            vert_x_cal=column_stack((plx,plx_cal))
            vert_y_cal=column_stack((ply,ply_cal))

    ### Put a maximum of max_n injection in each plot to improve readability
    max_n=25
    d,r=divmod(len(plx),max_n)
    inverted_seqs=[(range(len(plx))[::-1])[k*max_n:(k+1)*max_n] for k in range(d+1)]
    for seq in inverted_seqs:
        if seq!=[]:
            myfig=plt.figure(2,figsize=(20,28),dpi=200)
            plt.clf()
            for i in seq:
                #print i, range(len(plx)),bsn_cal[i]
                if bsn_cal[i]>4.0:
                    new_label=len(plx)-range(len(plx)).index(i)-1
                    #print bsn_cal[new_label],new_label
                    plt.plot(vert_x_cal[i,:],vert_y_cal[i,:],'g:',linewidth=1)
                    plt.plot(vert_x_ctrl[i,:],vert_y_ctrl[i,:],'y:',linewidth=1)
                    plt.annotate(str(new_label), color='k',xy=(vert_x_cal[i,0], vert_y_cal[i,0]), xytext=(vert_x_cal[i,0]*(1+1/100), vert_y_cal[i,0]*(1+1/150)),size=13,alpha=0.8)
                    plt.annotate(str(new_label), color='r',xy=(vert_x_cal[i,1], vert_y_cal[i,1]), xytext=(vert_x_cal[i,1]*(1+1/100), vert_y_cal[i,1]*(1+1/150)),size=13,alpha=0.8)
                    plt.annotate(str(new_label), color='b',xy=(vert_x_ctrl[i,0], vert_y_ctrl[i,0]), xytext=(vert_x_ctrl[i,0]*(1+1/100), vert_y_ctrl[i,0]*(1+1/150)),size=13,alpha=0.8)
                else:
                    continue

            plt.scatter(plx,ply,s=7,c='k',marker='d',faceted=False,label='Injected')
            plt.scatter(plx_cal,ply_cal,s=7,c='r',marker='o',faceted=False,label='Recovered_cal') 
            plt.scatter(plx_ctrl,ply_ctrl,s=7,c='b',marker='o',faceted=False,label='Recovered_ctrl')
            m.drawmapboundary()
            m.drawparallels(np.arange(-90.,120.,45.),labels=[1,0,0,0],labelstyle='+/-')
            # draw parallels
            m.drawmeridians(np.arange(0.,360.,90.),labels=[0,0,0,1],labelstyle='+/-')
            # draw meridians
            plt.legend(loc=(0,-0.1), ncol=3,mode="expand",scatterpoints=2)
            plt.title("Injected vs recovered positions")
            myfig.savefig(os.path.join(path_plots,'injected_skymap_%i.png'%inverted_seqs.index(seq)))
            plt.clf()
            
    return d,r

def histN(mat,N):
    Nd=size(N)
    histo=zeros(N)
    scale=array(map(lambda a,b:a/b,map(lambda a,b:(1*a)-b,map(max,mat),map(min,mat)),N)) ### takes mat and calculates (max_omega - min_omega)/N, scale is a Nd-dims array
    axes=array(map(lambda a,N:linspace(min(a),max(a),N),mat,N)) ### takes man and N and returns the axes
    print shape(axes)
    bins=floor(map(lambda a,b:a/b , map(lambda a,b:a-b, mat, map(min,mat) ),scale*1.01))
    hbins=reshape(map(int,bins.flat),bins.shape)
    print str(bins.flat)
    for co in transpose(hbins):
        t=tuple(co)
        histo[t[::-1]]=histo[t[::-1]]+1
    return (axes,histo)

def RunsCompare(outdir,inputs,inj,raw_events,IFOs,snrs=None,calerr=None,path_to_result_pages=None,keyword=None):
    
    from pylal import SimInspiralUtils
    
    checkDir(outdir)
    number_of_inits=len(inputs)
    injTable=SimInspiralUtils.ReadSimInspiralFromFiles([inj])
    flow=20.0
    fup=500.0
    label_size=22
    time_event=None
    ### Read the trigger times from the injfile using the raw_event option
    if inj and raw_events:
        time_event={}
        events=[]
        times=[]
        raw_events=raw_events.replace('[','').replace(']','').split(',')
        if 'all' is raw_events or 'all' in raw_events:
            events=range(len(injTable))

        else:
            for raw_event in raw_events:
                if ':' in raw_event:
                    limits=raw_event.split(':')
                    if len(limits) != 2:
                        print "Error: in event config option; ':' must separate two numbers."
                        exit(0)
                    low=int(limits[0])
                    high=int(limits[1])+1   # The plus one is to include the rigthmost extremum
                    if low>high:
                        events.extend(range(int(high),int(low)))
                    elif high>low:
                        events.extend(range(int(low),int(high)))
                else:
                    events.append(int(raw_event))

        for event in events:
            times.append(injTable[event].get_end())
            time_event[times[-1]]=event

        starttime=min(times)-1.0
        endtime=max(times)+1.0
    else:
        print "Error, you must give --events and --inj option \n"

    ### Combine path contains both the weighted posteriors and the BSN files
    BSN=[path for path in inputs]
    Combine=[path for path in inputs]
    snrs=[path for path in snrs]
    temp_times=[time for time in times]
    ## Remove the times for which posterior file is not present in either of the init ## TBD I only need to compare couple of ctrl-cali, not all of them
    for run in range(len(Combine)):
        for time in times:
            path_to_file=os.path.join(Combine[run],'posterior_samples_'+str(time)+'.000')
            if not os.path.isfile(path_to_file):
                temp_times.remove(time)
        times=[time for time in temp_times]
    if times==[]:
        print "No posteriors found for the events requested. Exiting...\n"
        sys.exit(1)

    recovered_positions_cal={}
    injected_positions=[]
    recovered_positions_ctrl=[]
    ## prepare files with means and other useful data. It also fills the list with the sky positions ###
    for run in range(len(Combine)):
        if int(run)==0:
            summary=open(os.path.join(outdir,'summary_ctrl.dat'),'w')
        else:
            summary=open(os.path.join(outdir,'summary_'+str(run)+'.dat'),'w')
        header=open(os.path.join(outdir,'headers_'+str(run)+'.dat'),'w')
        header_l=[]
        header_l.append('injTime ')
        recovered_positions_cal[int(run)]=[]
        for time in times:
            path_to_file=os.path.join(Combine[run],'posterior_samples_'+str(time)+'.000')
            posterior_file=open(path_to_file,'r')
            peparser=bppu.PEOutputParser('common')
            commonResultsObj=peparser.parse(posterior_file)
            pos = bppu.Posterior(commonResultsObj,SimInspiralTableEntry=injTable[times.index(time)])
            posterior_file.close()
            parameters=pos.names            
            parameters.remove('logl')
            summary.write(str(time)+'\t')
            #if 'ra' in parameters and 'dec' in parameters:
            #    if int(run)==0:
            #        recovered_positions_ctrl.append([pos['dec'].mean,pos['ra'].mean])
            #        injected_positions.append([pos['dec'].injval,pos['ra'].injval])
            #    else:
            #        recovered_positions_cal[int(run)].append([pos['dec'].mean,pos['ra'].mean])
            for parameter in parameters:
                summary.write(repr(pos[parameter].mean) + '\t'+ repr(pos[parameter].stdev) +'\t')
                if time==times[0]:
                    header_l.append('mean_'+parameter)
                    header_l.append('stdev_'+parameter)
            if BSN is not None:
                path_to_file=os.path.join(BSN[run],'bayesfactor_'+str(time)+'.000.txt')
                bfile=open(path_to_file,'r')
                bsn=bfile.readline()[:-1] ## remove the newline tag
                bfile.close()
                summary.write(str(bsn)+'\t')
                if time==times[0]:
                    header_l.append('BSN')
            if 'ra' in parameters and 'dec' in parameters:
                if int(run)==0:
                    recovered_positions_ctrl.append([pos['dec'].mean,pos['ra'].mean,float(bsn)])
                    injected_positions.append([pos['dec'].injval,pos['ra'].injval,999])
                else:
                    recovered_positions_cal[int(run)].append([pos['dec'].mean,pos['ra'].mean,float(bsn)])
            #print recovered_positions_cal
            if snrs is not None:
                val,hea=read_snr(snrs,run,time,IFOs)
                summary.write(str(val)+'\t')
                if time==times[0]:
                    for he in hea:
                        header_l.append(he)
            summary.write('\n')
        header.write('\t'.join(str(n) for n in header_l)+'\n')
        summary.close()
        header.close()
    ### For the moment I'm only using a single header file. TBD: reading headers for all the runs and checking whether the parameters are consistent. Act smartly
    
    
    ### Now read the ctrl data, these stay the same all along while the cal_run data are read at each interation in the for below
    path_uncal=os.path.join(outdir,'summary_ctrl.dat')    
    for run in range(1,len(Combine)):
        run=str(run)
        path_cal=os.path.join(outdir,'summary_'+run+'.dat')
        MakePlots(outdir,path_cal,path_uncal,run,parameters,label_size,header_l)
        if snrs is not None:
            MakeSNRPlots(outdir,snrs,path_cal,path_uncal,run,parameters,header_l,IFOs,label_size)
        if BSN is not None and snrs is not None:
            MakeBSNPlots(outdir,path_cal,path_uncal,run,header_l,label_size,keyword)
        if calerr is not None:
            MakeErrorPlots(times[0],outdir,calerr,run,flow,fup,IFOs,label_size,keyword)
        if injected_positions!=[] and recovered_positions_cal!={}:
            d,r=Make_injected_sky_map(injected_positions,outdir,run,dec_ra_cal=recovered_positions_cal[int(run)],dec_ra_ctrl=recovered_positions_ctrl)

        WritePlotPage(outdir,run,parameters,times[0])
        WriteSummaryPage(outdir,run,path_to_result_pages[int(run)],path_to_result_pages[0],header_l,times,IFOs,keyword,d)

def MakeErrorPlots(time,outdir,in_data_path,run,f_0,f_up,IFOs,label_size,key):
    """
    @time: the injection time for which to plot the errors. Right now any time is good as the calibration errors are the same for all the injections in the xml
    @outir: will save plots in outdir/run/ErrorPlots
    @in_data_path a list of dirs. The Ith element points to the folder containing the calibration errors data of the Ith parser (I may change this behaviour, passing directly the right path)
    @run the number of the current init 
    @f_0 f_up the xlims of the plot, for the moment they are hardcoded
    @IFOs: IFOs to add in the plots
    @label_size: the size of the ticks' labels, labels, etc
    @key label to identify the non-control run
    Produces two plots: one with ratio of the amplitude with calibration over the amplitude of the control run and another with the difference of phases
    """
    run=str(run)
    path_plots=os.path.join(outdir,run,'ErrorPlots')
    checkDir(path_plots)
    data={}
    for IFO in IFOs:
        path_to_data=os.path.join(in_data_path[int(run)-1],'calerr_'+IFO+'_'+str(time)+'.0.dat')
        data[IFO]=np.loadtxt(path_to_data)
    a=0
    for i in range(len(data[IFOs[0]][:,0])):
        if fabs(data[IFOs[0]][i,0]-f_0)<0.0001:
            a=i
            continue 
    if a==0:
        print "Could not fix f_low to %5.2f. Exiting...\n"%f_0
        sys.exit(1)
    b=0
    for i in range(len(data[IFOs[0]][a:,0])+a):
        if fabs(data[IFOs[0]][i,0]-f_up)<0.0001:
            b=i
            continue
    if b==0:
        print "Could not fix f_up to %5.2f. Exiting...\n"%f_up
        sys.exit(1)
    
    myfig=figure(1,figsize=(10,8),dpi=80)
    ax=myfig.add_subplot(111)
    for (IFO,color) in zip(IFOs,['r','b','k']):
        plot(data[IFO][a:b,0],data[IFO][a:b,1],color,label=IFO)
    ax.set_xlabel('f[Hz]',fontsize=label_size)
    ax.set_ylabel('Amp_'+key+'/Amp_ctrl',fontsize=label_size)
    set_fontsize_in_ticks(ax,label_size)
    grid()
    legend()
    myfig.savefig(os.path.join(path_plots,'amp_'+str(time)+'.png'))
    myfig.clear()
    phase={}
    phase_normalized={}
    
    ### This function normalize the phase difference in the range [-Pi,Pi]
    def normalizer(phase,phase_normalized):
        for pha in phase:
            if pha<-1.5*pi:
                phase_normalized.append(pha+2*pi)
            elif pha>pi:
                phase_normalized.append(pha-2*pi)
            else:
                phase_normalized.append(pha)

    for IFO in IFOs:
        phase[IFO]=data[IFO][:,2]
        phase_normalized[IFO]=[]
        normalizer(phase[IFO],phase_normalized[IFO])

    myfig=figure(1,figsize=(10,8),dpi=80)
    ax=myfig.add_subplot(111)
    for (IFO,color) in zip(IFOs,['r','b','k']):
        plot(data[IFO][a:b,0],phase_normalized[IFO][a:b],color,label=IFO)
    ax.set_xlabel('f[Hz]',fontsize=label_size)
    ax.set_ylabel('Pha_'+key+' -Pha_ctrl [Rads]',fontsize=label_size)
    set_fontsize_in_ticks(ax,label_size)
    legend()
    grid()
    myfig.savefig(os.path.join(path_plots,'pha_'+str(time)+'.png'))
    myfig.clear()


def MakePlots(outdir,path_cal,path_uncal,run,parameters,label_size,header_l):
    nbins=20
    data_cal=np.loadtxt(path_cal)
    data_uncal=np.loadtxt(path_uncal)
    path_plots=os.path.join(outdir,run,'ParametersPlots')
    checkDir(path_plots)
    #bsn_cal=[bsn for bsn in data_cal[:,header_l.index('BSN')]]
    network_snrs_key=[snr for snr in data_cal[:,header_l.index('SNR_Network')]]
    theres_snr_ind=[]
    for snr in network_snrs_key:
        if snr>8.0:
            theres_snr_ind.append(network_snrs_key.index(snr))
    print theres_snr_ind
    
    for parameter in parameters:
        x=parameters.index(parameter)*2+1
        x_delta=(data_cal[theres_snr_ind,x]-data_uncal[theres_snr_ind,x])
        x_points=(map(lambda t:(min(x_delta) + (t/max(x_delta))*(max(x_delta)-min(x_delta))),x_delta)).sort
        x_points2=linear_space(min(x_delta),max(x_delta),len(x_delta))
        myfig=figure(2,figsize=(10,10),dpi=80)
        ax=myfig.add_subplot(111)
        ax.plot(x_delta,data_uncal[theres_snr_ind,x+1],'.r',label='stdev')
        axvline(x=0, ymin=0, ymax=1,linewidth=2, color='b')
        plot(x_points2,fabs(x_points2)*2,'-k',label='$0.5 \sigma$')
        plot(x_points2,fabs(x_points2),'-y',label='$\sigma$')
        plot(x_points2,fabs(x_points2)/2,'-c',label='2$\sigma$')
        set_fontsize_in_ticks(ax,label_size)
        ax.set_xlabel("delta_"+str(parameter),fontsize=label_size)
        ax.set_ylabel("sigma_"+str(parameter),fontsize=label_size)
        grid()
        #legend(loc="2")
        myfig.savefig(os.path.join(path_plots,"delta_sigma_"+str(parameter)+'.png'))
        myfig.clear()
        mean_delta_omega=mean(x_delta)
        std_delta_omega=std_dev(x_delta,mean_delta_omega)
        skewness_delta_omega= skewness(x_delta, mean_delta_omega, std_delta_omega)
        kurtosis_delta_omega= kurtosis(x_delta, mean_delta_omega, std_delta_omega)
        #print "Mean : %e\n" % mean_delta_omega
        #print "Standard Deviation %e\n" % std_delta_omega
        #print "Skewness %e\n" % skewness_delta_omega
        #print "Kurtosis %e\n" % kurtosis_delta_omega
        
        bins=linear_space(x_delta.min(),x_delta.max(),nbins)
        myfig=figure(figsize=(4,3.5),dpi=80)
        xlabel(u"\u0394"+ parameter)
        hist(x_delta, bins=bins, normed="true")
        axvline(x=0, ymin=0, ymax=1,linewidth=2, color='r')
        grid()
        myfig.savefig(os.path.join(path_plots,'delta_'+parameter+'.png'))
        myfig.clear()
        ##### This part calculates effect size
        effect_size =(data_cal[theres_snr_ind,x]-data_uncal[theres_snr_ind,x])/data_uncal[theres_snr_ind,x+1]  
        mean_effect_size=mean(effect_size)
        std_effect_size=std_dev(effect_size,mean_effect_size)
        for i in range(len(data_cal[:,0])):
            if (data_cal[i,x]-data_uncal[i,x])/data_uncal[i,x+1]>3.0:
                print "parameter ",parameter, i,data_cal[i,x],data_uncal[i,x],data_uncal[i,x+1],(data_cal[i,x]-data_uncal[i,x])/data_uncal[i,x+1]
        skewness_effect_size= skewness(effect_size, mean_effect_size, std_effect_size)
        kurtosis_effect_size= kurtosis(effect_size, mean_effect_size, std_effect_size)
        print "Mean %s in run %i: %e\n" %(parameter,int(run),mean_effect_size)
        print "Standard Deviation  %s in run %i: %e\n" %(parameter,int(run),std_effect_size)
        print "Median %s in run %i: %e\n" %(parameter,int(run),np.median(effect_size))
        print "50perc %s in run %i: %e\n" %(parameter,int(run),stat.scoreatpercentile(effect_size,50))
        print "5perc %s in run %i: %e\n" %(parameter,int(run),stat.scoreatpercentile(effect_size,5))
        print "95 perc  %s in run %i: %e\n" %(parameter,int(run),stat.scoreatpercentile(effect_size,95))

        #print "Kurtosis %e\n" % kurtosis_effect_size
        bins=linear_space(effect_size.min(),effect_size.max(),nbins)
        myfig2=figure(figsize=(4,3.5),dpi=80)
        hist(effect_size, bins=bins, normed="true",color='r',fill=False, hatch='//', linewidth='2')
        xlabel('effect_'+parameter)
        axvline(x=0, ymin=0, ymax=1,linewidth=2, color='b')
        grid()
        legend()
        myfig2.savefig(os.path.join(path_plots,'effect_'+parameter+'.png'))
        myfig2.clear()
        
def MakeSNRPlots(outdir,snrs,path_cal,path_uncal,run,parameters,header_l,IFOs,label_size):
    
    data_cal=np.loadtxt(path_cal)
    data_ctrl=np.loadtxt(path_uncal)
    path_plots=os.path.join(outdir,run,'SNRPlots')
    checkDir(path_plots)
    network_snrs=data_cal[:,header_l.index('SNR_Network')]
    network_snrs_ctrl=data_ctrl[:,header_l.index('SNR_Network')]
    bsn_cal=data_cal[:,header_l.index('BSN')]
    #bsn_cal=[bsn for bsn in data_cal[:,header_l.index('BSN')]]
    network_snrs_key=[snr for snr in data_cal[:,header_l.index('SNR_Network')]]
    theres_snr_ind=[]
    for snr in network_snrs_key:
        if snr>8.0:
            theres_snr_ind.append(network_snrs_key.index(snr))
    print theres_snr_ind
    #bsn_cal=np.asarray(bsn_cal)
    for parameter in parameters:
        i=parameters.index(parameter)*2+1
        y_effect=(data_cal[theres_snr_ind,i]-data_ctrl[theres_snr_ind,i])/data_ctrl[theres_snr_ind,i+1]        
        y_delta=(data_cal[theres_snr_ind,i]-data_ctrl[theres_snr_ind,i])
        myfig=plt.figure(2,figsize=(10,10),dpi=80)
        ax=myfig.add_subplot(211)
        ax.plot(network_snrs[theres_snr_ind],y_effect,'bo',label='EffectVsSNR')
        ax.set_xlabel('Network SNR cal',fontsize=label_size)
        ax.set_ylabel('effect_%s'%parameter,fontsize=label_size)
        locs, labels = (ax.get_xticks(),ax.get_xticklabels)
        set_fontsize_in_ticks(ax,label_size)
        ax.legend()
        grid()
        ax2=myfig.add_subplot(212)
        #ax2.set_xticks(locs)
        #ax2.set_xticklabels(['%4.1f'%a for a in np.linspace(min(bsn_cal),max(bsn_cal),len(locs))])
        ax2.set_xlabel('BSN cal',fontsize=label_size)
        #set_fontsize_in_ticks(ax2,label_size)
        #ax3=ax.twinx()
        #y_effect=(data_cal[:,i]-data_ctrl[:,i])/data_ctrl[:,i+1]        
        ax2.plot(bsn_cal[theres_snr_ind],y_effect,'ro',label='EffectVsBSN')
        ax2.set_ylabel('effect_%s'%parameter,fontsize=label_size)
        set_fontsize_in_ticks(ax2,label_size)        
        ax2.legend()
        ax2.grid()
        myfig.savefig(os.path.join(path_plots,'SNR_vs_'+parameter+'.png'))
        myfig.clear()
    return

def set_fontsize_in_ticks(axes,size):
    [t.set_fontsize(size) for t in axes.xaxis.get_ticklabels()]
    [t.set_fontsize(size) for t in axes.yaxis.get_ticklabels()]

def MakeBSNPlots(outdir,path_cal,path_uncal,run,header_l,label_size,key):
    labelsize=22
    data_cal=np.loadtxt(path_cal)
    data_ctrl=np.loadtxt(path_uncal)
    path_plots=os.path.join(outdir,run,'BSNPlots')
    checkDir(path_plots)
    network_snrs=data_cal[:,header_l.index('SNR_Network')]
    network_snrs_ctrl=data_ctrl[:,header_l.index('SNR_Network')]
    bsns_cal=data_cal[:,header_l.index('BSN')]
    bsns_ctrl=data_ctrl[:,header_l.index('BSN')]
    myfig=figure(2,figsize=(10,10),dpi=80)
    ax=myfig.add_subplot(111)
    ax.plot(network_snrs,bsns_cal,'ro',label='BSN_'+key)
    ax.set_xlabel('Network SNR '+key,fontsize=label_size)
    ax.set_ylabel('$\mathrm{log\,B}$',fontsize=label_size)
    locs, labels = (ax.get_xticks(),ax.get_xticklabels)
    grid()
    #ax2=ax.twiny()
    #ax2.set_xticks(locs)
    ax.plot(network_snrs_ctrl,bsns_ctrl,'bo',label='BSN_ctrl')
    #ax2.legend(loc='upper left')
    ax.legend(loc='upper right')
    #ax2.set_xticklabels(['%4.1f'%a for a in np.linspace(min(network_snrs_ctrl),max(network_snrs_ctrl),len(locs))])
    #ax2.set_xlabel('Network SNR ctrl',fontsize=label_size)
    myfig.savefig(os.path.join(path_plots,'BSN_vs_SNR.png'))
    myfig.clear()

def WritePlotPage(outdir,run,parameters,first_time):
    run=str(run)
    first_time=str(first_time)
    wd=300
    hg=250
    abs_page_path=os.path.join(outdir,run)
    page_path="./"
    path_plots=os.path.join(page_path,'ParametersPlots')
    error_path_plots=os.path.join(page_path,'ErrorPlots')
    snr_plots=os.path.join(page_path,'SNRPlots')
    bsn_plots=os.path.join(page_path,'BSNPlots')
    sky_plots=os.path.join(page_path,'SkyPlots')
    html=bppu.htmlPage('CalibrationErrors',css=bppu.__default_css_string)
    html_err=html.add_section('Errors fit')
    html_err_st='<table><tr>'
    for plot in ['amp_','pha_']:
        html_err_st+='<td>'
        if os.path.isfile(os.path.join(abs_page_path,'ErrorPlots',plot +first_time+'.png')):
            html_err_st+=linkImage(os.path.join(error_path_plots,plot +first_time+'.png'),1.5*wd,1.5*hg)
        else:
            html_err_st+='<p> No calibration error curves found in ' + error_path_plots +'</p>'
        html_err_st+='</td>'
    html_err_st+='</tr></table>'
    html_err.write(html_err_st)

    html_plots=html.add_section('Summary plots')
    html_plots.write(link(os.path.join(page_path,'summary.html'),"Go to the summary table"))
    html_plots_st='<table>'
    for parameter in parameters:
        html_plots_st+='<tr>'
        for plot in ['delta_','effect_','delta_sigma_']:
            html_plots_st+='<td>'
            html_plots_st+=linkImage(os.path.join(path_plots,plot +parameter+'.png'),wd,hg)
            html_plots_st+='</td>'
        for plot in ['SNR_vs_']:
            html_plots_st+='<td>'
            html_plots_st+=linkImage(os.path.join(snr_plots,plot +parameter+'.png'),wd,hg)
            html_plots_st+='</td>'
        html_plots_st+='</tr>'
    html_plots_st+='<tr><td colspan="2">'
    html_plots_st+=linkImage(os.path.join(bsn_plots,'BSN_vs_SNR.png'),2*wd,2*hg)
    html_plots_st+='</td><td colspan="2">'
    html_plots_st+=linkImage(os.path.join(sky_plots,'injected_skymap.png'),2*wd,2*hg)
    html_plots_st+='</td></tr>'
    html_plots_st+='</table>'
    html_plots.write(html_plots_st) 
    #Save results page
    plotpage=open(os.path.join(abs_page_path,'posposplots.html'),'w')
    plotpage.write(str(html))
    return

def go_home(path):
    current=os.getcwd()
    upo=''
    os.chdir(path)
    while os.getcwd()!=os.environ['HOME']:
        os.chdir(os.path.join(os.getcwd(),'../'))
        upo+='../'
    os.chdir(current)
    return upo

def WriteSummaryPage(outdir,run,path_to_result_pages,path_to_ctrl_result_pages,header_l,times,IFOs,key,d):
    
    wd=500
    hg=400
    run=str(run)
    abs_page_path=os.path.join(outdir,run)
    page_path="./"
    path_to_sky=os.path.join(page_path,'SkyPlots')
    path_to_result_pages_from_LSC=path_to_result_pages[path_to_result_pages.find('LSC'):]
    path_to_ctrl_result_pages_from_LSC=path_to_ctrl_result_pages[path_to_ctrl_result_pages.find('LSC'):]
    snr_index={}
    ifos=['SNR_'+ifo for ifo in IFOs]
    ifos.append('SNR_Network')

    for ifo in ifos:
        snr_index[ifo]=header_l.index(ifo)  

    bsn_index=header_l.index('BSN')
    ctrl_data=np.loadtxt(os.path.join(outdir,'summary_ctrl.dat'))
    cal_data=np.loadtxt(os.path.join(outdir,'summary_'+run+'.dat'))

    col_num=3
    html=bppu.htmlPage('SummaryPage',css=bppu.__default_css_string)
    html_sky=html.add_section('Skymaps')
    html_sky_str="<table><tr>"
    for i in range(d+1):
        if os.path.isfile(os.path.join(abs_page_path,'SkyPlots','injected_skymap_'+str(i)+'.png')):
            html_sky_str+='<td>'+linkImage(os.path.join(path_to_sky,'injected_skymap_'+str(i)+'.png'),wd,hg)+'</td>'
        else:
            html_sky_str+='<td> </td>'
        if (range(d+1).index(i)+1)%col_num==0 and i!=d:
            html_sky_str+='</tr><tr>'
    html_sky_str+='</tr></table>'
    print html_sky_str,range(d+1),range(d+1).index(i)
    html_sky.write(html_sky_str)

    html_table=html.add_section('Links to postprocessing pages')
    html_table.write(link(os.path.join(page_path,'posposplots.html'),"Go back to the plots page"))
    html_table_st='<table>'
    html_table_st+='<tr><th align="center" colspan="6"> Control Runs </th><th colspan="6" align="center"> '+key.title()+' Runs </th></tr>'
    html_table_st+='<tr>'
    html_table_st+=2*('<th> TriggerTime</th><th> BSN </th><th>'+ifos[0]+'</th><th>'+ifos[1]+'</th><th>'+ifos[2]+'</th><th>'+ifos[3]+'</th>')
    html_table_st+='</tr>'

    for time in times:
        time=str(time)
        html_table_st+='<tr>'
        ctrl_page=os.path.join(go_home(outdir),path_to_ctrl_result_pages_from_LSC,time,'posplots.html')
        cal_page=os.path.join(go_home(outdir),path_to_result_pages_from_LSC,time,'posplots.html')
        html_table_st+='<td>'+link(ctrl_page,time)+'</td>'
        html_table_st+='<td>'+'%4.2f'%(ctrl_data[times.index(time),bsn_index])+'</td>'
        for IFO in ifos:
            html_table_st+='<td>'+'%4.2f'%(ctrl_data[times.index(time),snr_index[IFO]])+'</td>'
        html_table_st+='<td>'+link(cal_page,time)+'</td>'
        html_table_st+='<td>'+'%4.2f'%(cal_data[times.index(time),bsn_index])+'</td>'
        for IFO in ifos:
            html_table_st+='<td>'+'%4.2f'%(cal_data[times.index(time),snr_index[IFO]])+'</td>'
        html_table_st+='</tr>'
    html_table_st+='</table>'
    html_table.write(html_table_st)
    #Save results page
    plotpage=open(os.path.join(abs_page_path,'summary.html'),'w')
    plotpage.write(str(html))

def vararg_callback(option, opt_str, value, parser):
    assert value is None
    value = []
    def floatable(str):
        try:
            float(str)
            return True
        except ValueError:
            return False
    for arg in parser.rargs:
        # stop on --foo like options
        if arg[:2] == "--" and len(arg) > 2:
            break
        # stop on -a, but not on -3 or -3.0
        if arg[:1] == "-" and len(arg) > 1 and not floatable(arg):
            break
        value.append(arg)
    del parser.rargs[:len(value)]
    setattr(parser.values, option.dest, value)        

if __name__=='__main__':
    from optparse import OptionParser
    parser=OptionParser()
    parser.add_option("-o","--outpath", dest="outpath",type="string",help="make page and plots in DIR", metavar="DIR")
    parser.add_option("-d","--data", dest="indata",action="callback", callback=vararg_callback,help="The folders containing the posteriors and the BSN files of the runs", metavar="PathToPosterior1 PathToPosterior2 etc")
    parser.add_option("-s","--snr", dest="snr",action="callback", callback=vararg_callback,help="The folders containing the snrs of the runs",metavar="pathToSnr1 pathToSnr2 etc")
    parser.add_option("-r","--result_pages_path",default=None,dest="rp",action="callback",callback=vararg_callback,help="Paths to the folder containing the postplots pages (this folder must contain the timebins folders)",metavar="r")
    parser.add_option("-i","--inj",dest="inj",action="store",type="string",default=None,help="Injection xml table",metavar="injection.xml")
    parser.add_option("-e","--calerr",dest="calerr",action="callback",callback=vararg_callback,default=None,help="path to calibration errors path",metavar="/pathToCalerr1 /pathToCalerr2 etc")
    parser.add_option("-E","--events",dest="raw_events",action="store",type="string",default=None,metavar="[0:50]")
    parser.add_option("-I","--IFOS",dest="IFOs",action="callback", callback=vararg_callback,help="The IFOs used in the analysis", metavar="H1 L1 V1")
    parser.add_option("-k","--keyword",dest="key",action="store",type="string",default=None,help="This is the work that characterize the non-control runs (eg. calibration). It will be used to label various things (plots' labels, filenames, etc)", metavar="non-control-word")
    (opts,args)=parser.parse_args()
    #if opts.num_of_init==1 and opts.uncal_path==None:
    #        print "Error, if -n is 1 it means that only jobs with calibration errors are running, then you must provide the path to the posterior of the (already run) corresponding jobs without calibration errors using the option -u /pathToUncalPosteriors \n"
    RunsCompare(opts.outpath,opts.indata,opts.inj,opts.raw_events,opts.IFOs,snrs=opts.snr,calerr=opts.calerr,path_to_result_pages=opts.rp,keyword=opts.key)
