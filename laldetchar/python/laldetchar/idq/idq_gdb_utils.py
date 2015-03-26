# Copyright (C) 2014 Reed Essick
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

## \addtogroup laldetchar_py_idq
## Synopsis
# ~~~
# from laldetchar.idq import idq_gdb_utils
# ~~~
# \author Reed Essick (<reed.essick@ligo.org>)

"""Module with utility functions used for generating iDQ input to GraceDB.
"""

from laldetchar import git_version

__author__ = 'Reed Essick <reed.essick@ligo.org>'
__version__ = git_version.id
__date__ = git_version.date

## \addtogroup laldetchar_py_idq_auxmvc
# @{

# Utility functions used for generating iDQ input to GraceDB.

import numpy as np
import re as re
from laldetchar.idq import event
from laldetchar.idq import idq

def execute_gdb_timeseries(
    gps_start,
    gps_end,
    gps,
    gracedb_id,
    ifo,
    classifier,
    cp,
    input_dir,
    exec_prog,
    usertag='',
    gch_xml=[],
    cln_xml=[],
    plotting_gps_start=None,
    plotting_gps_end=None):
    """ Function that sets up and runs idq-gdb-timeseries script as one of the tasks of idq-gdb-processor."""
    # form the command line
    cmd_line = [exec_prog, '-s', gps_start, '-e', gps_end, '--gps', gps,\
        '-g', gracedb_id, '--ifo', ifo, '-c', classifier, '-i', input_dir,\
        '-t', usertag]
	
    # add extra options from config file
    if cp.has_option("general","gdb_url"):
        cmd_line += ["--gdb-url", cp.get("general","gdb_url")]
    if plotting_gps_start:
        cmd_line += ["--plotting-gps-start", str(plotting_gps_start)]
    if plotting_gps_end:
        cmd_line += ["--plotting-gps-end", str(plotting_gps_end)]
    for gch in gch_xml:
        cmd_line += ["--gch-xml", gch]
    for cln in cln_xml:
        cmd_line += ["--cln-xml", cln]

    for (option,value) in cp.items('gdb-time-series'):
        cmd_line.extend([option, value])
	print cmd_line
    exit_status = idq.submit_command(cmd_line, 'gdb_timeseries', verbose=True)
	
    return exit_status
	
def execute_gdb_glitch_tables(
    gps_start,
    gps_end,
    gracedb_id,
    ifo,
    classifier,
    cp,
    input_dir,
    exec_prog,
    usertag=''):
    """ Function that sets up and runs idq-gdb-timeseries script as one of the tasks of idq-gdb-processor."""
    # form the command line
    cmd_line = [exec_prog, '-s', gps_start, '-e', gps_end, '-g', gracedb_id,\
        '--ifo', ifo, '-c', classifier, '-i', input_dir, '-t', usertag]
	
    # add extra options from config file
    if cp.has_option("general","gdb_url"):
        cmd_line += ["--gdb-url", cp.get("general","gdb_url")]
    for (option,value) in cp.items('gdb-glitch-tables'):
        cmd_line.extend([option, value])
	print cmd_line
    exit_status = idq.submit_command(cmd_line, 'gdb_glitch_tables', verbose=True)
	
    return exit_status	
##@}

