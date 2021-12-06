# ReuseTracker

## Requirement

Linux kernel version 5.0.0 or higher

## Installation

1. Install hpctoolkit-externals from https://github.com/WitchTools/hpctoolkit-externals
by typing the following command in the directory of hpctoolkit-externals:
        ./configure && make && make install
2. Install the custom libmonitor from https://github.com/WitchTools/libmonitor
by typing the following command in the directory of libmonitor:
        ./configure \-\-prefix=\<libmonitor-installation directory\> && make && make install
3. Install HPCToolkit with ReuseTracker extensions from
	https://github.com/ParCoreLab/hpctoolkit pointing to the installations of hpctoolkit-externals and libmonitor from steps \#1 and \#2. Assuming that the underlying architecture is x86_64 and compiler is gcc, this step is performed with the following commands.

	a. ./configure \-\-prefix=\<targeted installation directory for ComDetective\> \-\-with-externals=\<directory of hpctoolkit externals\>/x86_64-unknown-linux-gnu \-\-with-libmonitor=\<libmonitor-installation directory\>

	b. make

	c. make install

## Usage

To run perf_event_open system call without having to use sudo access,
set the value of perf_event_paranoid to -1 by typing the following command:
sudo sysctl -w kernel.perf_event_paranoid=-1

1. Compile the code that you want to profile using "-g" flag to allow for debugging.</li>

2. To run ReuseTracker to profile reuse distance in private caches: 

	HPCRUN_WP_REUSE_PROFILE_TYPE=\<"TEMPORAL"\|"SPATIAL"\> HPCRUN_PROFILE_L3=false HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name of output folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling period\> <./your_executable> your_args



