# ReuseTracker

## Requirement

- Linux kernel version 5.0.0 or higher
- To run perf_event_open system call without having to use sudo access,
set the value of perf_event_paranoid to -1 by typing the following command:
sudo sysctl -w kernel.perf_event_paranoid=-1

## Installation

0. (In AMD) Install the Linux kernel module for IBS from https://github.com/ParCoreLab/AMD_IBS_Toolkit
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


## Usage on Intel

- To profile the private cache temporal reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name_of_output_folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling_period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling_period\> <./your_executable> your_args

- To profile the private cache spatial reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="SPATIAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name_of_output_folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling_period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling_period\> <./your_executable> your_args

- To profile the shared cache temporal reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=true HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name_of_output_folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling_period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling_period\> <./your_executable> your_args

- To profile the shared cache spatial reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="SPATIAL" HPCRUN_PROFILE_L3=true HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name_of_output_folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling_period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling_period\> <./your_executable> your_args


## Usage on AMD

- To profile the private cache temporal reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_AMD_REUSETRACKER -e IBS_OP@\<sampling_period\> -e AMD_L1_DATA_ACCESS@100000000 <./your_executable> your_args

- To profile the private cache spatial reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="SPATIAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_AMD_REUSETRACKER -e IBS_OP@\<sampling_period\> -e AMD_L1_DATA_ACCESS@100000000 <./your_executable> your_args

- To profile the shared cache temporal reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=true HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_AMD_REUSETRACKER -e IBS_OP@\<sampling_period\> -e AMD_L1_DATA_ACCESS@100000000 <./your_executable> your_args

- To profile the shared cache spatial reuse distance of an application, run the following command.

HPCRUN_WP_REUSE_PROFILE_TYPE="SPATIAL" HPCRUN_PROFILE_L3=true HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_AMD_REUSETRACKER -e IBS_OP@\<sampling_period\> -e AMD_L1_DATA_ACCESS@100000000 <./your_executable> your_args


## Attribution to Locations in Source Code

1. Compile the code that you want to profile using "-g" flag to allow for debugging.

2. To attribute the detected communications to their locations in source code lines and program stacks,
you need to take the following steps:

a. Download and extract a binary release of hpcviewer from
http://hpctoolkit.org/download/hpcviewer/latest/hpcviewer-linux.gtk.x86_64.tgz

b. Run ReuseTracker on a program to be profiled

In Intel:

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@100000 -e MEM_UOPS_RETIRED:ALL_STORES@100000 <./your_executable> your_args

In AMD:

HPCRUN_WP_REUSE_PROFILE_TYPE="TEMPORAL" HPCRUN_PROFILE_L3=false HPCRUN_WP_REUSE_BIN_SCHEME=4000,2 HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -e WP_AMD_REUSETRACKER -e IBS_OP@100000 -e AMD_L1_DATA_ACCESS@100000000 <./your_executable> your_args

c. Extract the static program structure from the profiled program by using hpcstruct

hpcstruct <./your_executable>

The output of hpcstruct is <./your_executable>.hpcstruct.

d. Generate an experiment result database using hpcprof

hpcprof -S <./your_executable>.hpcstruct -o <name of database> <name of output folder>

The output of hpcprof is a folder named <name of database>.

e. Use hpcviewer to read the content of the experiment result database in a GUI interface

hpcviewer/hpcviewer <name of database>

Information on program stack and source code lines is available in the Scope column,
and the other columns display numbers of detected reuses, total time distance,
and total reuse distance that correspond to the source code lines.
