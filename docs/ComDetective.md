# ComDetective

## Requirement

Linux kernel version 5.0.0 or higher

## Installation

1. Install hpctoolkit-externals from https://github.com/WitchTools/hpctoolkit-externals
by typing the following command in the directory of hpctoolkit-externals:
        ./configure && make && make install
2. Install the custom libmonitor from https://github.com/WitchTools/libmonitor
by typing the following command in the directory of libmonitor:
        ./configure \-\-prefix=\<libmonitor-installation directory\> && make && make install
3. Install HPCToolkit with ComDetective extensions from
	https://github.com/ParCoreLab/hpctoolkit pointing to the installations of hpctoolkit-externals and libmonitor from steps \#1 and \#2. Assuming that the underlying architecture is x86_64 and compiler is gcc, this step is performed with the following commands.

	a. ./configure \-\-prefix=\<targeted installation directory for ComDetective\> \-\-with-externals=\<directory of hpctoolkit externals\>/x86_64-unknown-linux-gnu \-\-with-libmonitor=\<libmonitor-installation directory\>

	b. make

	c. make install

## Usage

To run perf_event_open system call without having to use sudo access,
set the value of perf_event_paranoid to -1 by typing the following command:
sudo sysctl -w kernel.perf_event_paranoid=-1

1. Compile the code that you want to profile using "-g" flag to allow for debugging.</li> 

2. To run ComDetective with default configuration (sampling period: 500K, bulletin board size: 127, number of watchpoints: 4, and name of output folder "\<timestamp\>_timestamped_results"):

	ComDetectiverun <./your_executable> your_args

3. To run ComDetective with custom configuration (user-chosen sampling period, bulletin board size, 
number of watchpoints, minimum size of data objects to be detected, and name of output folder):

	ComDetectiverun \-\-period \<sampling rate\> \-\-bulletin-board-size \<bulletin board size\> \-\-debug-register-size \<number of debug registers\> \-\-object-size-threshold \<minimum number of bytes of detectable objects\> \-\-output \<name of output folder\> <./your_executable> your_args

	or

	ComDetectiverun -p \<sampling rate\> -b \<bulletin board size\> -d \<number of debug registers\> -t \<minimum number of bytes of detectable objects\> -o \<name of output folder\> <./your_executable> args_for_executable

	To monitor a program that has multiple processes (e.g. an MPI program):

	mpirun -n \<process count\> ComDetectiverun <./your_executable> your_args

To attribute the detected communications to their locations in source code lines and program stacks, you need to take the following steps:

1. Download and extract a binary release of hpcviewer from http://hpctoolkit.org/download/hpcviewer/latest/hpcviewer-linux.gtk.x86_64.tgz
 
2. Run ComDetective on a program to be profiled

	ComDetectiverun \-\-output <name of output folder> <./your_executable> your_args

3. Extract the static program structure from the profiled program by using hpcstruct

	hpcstruct <./your_executable>

	The output of hpcstruct is <./your_executable>.hpcstruct.

4. Generate an experiment result database using hpcprof

	hpcprof -S <./your_executable>.hpcstruct -o \<name of database\> \<name of output folder\>

	The output of hpcprof is a folder named \<name of database\>.

5. Use hpcviewer to read the content of the experiment result database in a GUI interface

	hpcviewer/hpcviewer \<name of database\>

	Information on program stack and source code lines is available in the Scope column, and
information about communication counts detected on the corresponding program stack and 
source code lines is available under "COMMUNICATION:Sum (I)" column.


## Communication Matrices and Communication Ranks of Data Objects

Communication matrices and ranking of data objects are dumped to the output folder. 
If you don't pass a name for the output folder with "--output" or "-o" parameter, 
the name of the output folder is "\<timestamp\>_timestamped_results". 

Each application level matrix file is named as follow: \<executable name\>-\<pid of the process\>-\<matrix type\>_matrix.csv, 
while data object level matrix file is named as follow: \<executable name\>-\<pid of the process\>-\<object id\>-\<matrix type\>\_matrix_rank\_<object rank\>.csv. 

\<matrix type\> can be "as" for any communication among threads, "ts" for true sharing among threads, 
"fs" for false sharing among threads, "as_core" for any communication among cores, 
"fs_core" for false sharing among cores, or "ts_core" for true sharing among cores. 
\<object id\> is associated with the corresponding data object's name in file \<executable name\>-\<pid of the process\>-\<matrix type\>_object_ranking.txt. 
In this txt file, all data objects are ranked with respect to the counts of communication whose type is indicated by the \<matrix type\>. 
Total counts of communications are printed in the log file named \<executable name\>-*.log within the output folder.


## Attribution of Communications to Data Objects

Please note that if you enable attribution of communications to data objects by following 
the instructions in ComDetective.Install, and try to detect every single dynamic memory allocation
by passing "-t 0" parameter to ComDetective, the profiled program can run slowly.  

The reason for this is that if the profiled program calls dynamic memory allocation functions 
a lot of times (like millions of mallocs), ComDetective can be slowed down 
as it intercepts these function calls and inserts info on the allocated memory ranges
to its database of dynamic objects. To get around this problem, ComDetective's user can accelerate
ComDetective by restricting it to detect only large objects. 
For example, instead of capturing every single dynamic memory allocation 
(with command line parameter "-t 0"), ComDetective can be restricted to capture only dynamic memory allocations 
with size 10000 bytes or higher (with command line parameter "-t 10000").
