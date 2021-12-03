# Using ComDetective

To run perf_event_open system call without having to use sudo access,
set the value of perf_event_paranoid to -1 by typing the following command:
sudo sysctl -w kernel.perf_event_paranoid=-1

<ol type="1">
<li>Compile the code that you want to profile using "-g" flag to allow for debugging.</li> 

<li>To run ComDetective with default configuration (sampling period: 500K, bulletin board size: 127, number of watchpoints: 4, and name of output folder "<timestamp>_timestamped_results"):

<br> ComDetectiverun <./your_executable> your_args</li>

<li>To run ComDetective with custom configuration (user-chosen sampling period, bulletin board size, 
number of watchpoints, minimum size of data objects to be detected, and name of output folder):

<br> ComDetectiverun --period <sampling rate> --bulletin-board-size <bulletin board size> --debug-register-size <number of debug registers> --object-size-threshold <minimum number of bytes of detectable objects> --output <name of output folder> <./your_executable> your_args

<br> or

<br> ComDetectiverun -p <sampling rate> -b <bulletin board size> -d <number of debug registers> -t <minimum number of bytes of detectable objects> -o <name of output folder> <./your_executable> args_for_executable</li>

<li>To monitor a program that has multiple processes (e.g. an MPI program):

<br>mpirun -n <process count> ComDetectiverun <./your_executable> your_args</li>

<li>To attribute the detected communications to their locations in source code lines and program stacks, you need to take the following steps:

<ol type="a">
<li> Download and extract a binary release of hpcviewer from http://hpctoolkit.org/download/hpcviewer/latest/hpcviewer-linux.gtk.x86_64.tgz </li>
 
<li> Run ComDetective on a program to be profiled

<br> ComDetectiverun --output <name of output folder> <./your_executable> your_args </li>

<li> Extract the static program structure from the profiled program by using hpcstruct

<br> hpcstruct <./your_executable>

<br> The output of hpcstruct is <./your_executable>.hpcstruct. </li>

<li> Generate an experiment result database using hpcprof

<br> hpcprof -S <./your_executable>.hpcstruct -o <name of database> <name of output folder>

<br> The output of hpcprof is a folder named <name of database>. </li>

<li> Use hpcviewer to read the content of the experiment result database in a GUI interface

<br> hpcviewer/hpcviewer <name of database>

<br> Information on program stack and source code lines is available in the Scope column, and
information about communication counts detected on the corresponding program stack and 
source code lines is available under "COMMUNICATION:Sum (I)" column.
</li>
</ol>
</li>
</ol>


# Communication Matrices and Communication Ranks of Data Objects

Communication matrices and ranking of data objects are dumped to the output folder. 
If you don't pass a name for the output folder with "--output" or "-o" parameter, 
the name of the output folder is "<timestamp>_timestamped_results". 

<br>
<br>
Each application level matrix file is named as follow: <executable name>-<pid of the process>-<matrix type>_matrix.csv, 
while data object level matrix file is named as follow: <executable name>-<pid of the process>-<object id>-<matrix type>_matrix_rank_<object rank>.csv. 

<br>
<br>

<matrix type> can be "as" for any communication among threads, "ts" for true sharing among threads, 
"fs" for false sharing among threads, "as_core" for any communication among cores, 
"fs_core" for false sharing among cores, or "ts_core" for true sharing among cores. 
<object id> is associated with the corresponding data object's name in file <executable name>-<pid of the process>-<matrix type>_object_ranking.txt. 
In this txt file, all data objects are ranked with respect to the counts of communication whose type is indicated by the <matrix type>. 
Total counts of communications are printed in the log file named <executable name>-*.log within the output folder.


# Attribution of Communications to Data Objects

Please note that if you enable attribution of communications to data objects by following 
the instructions in ComDetective.Install, and try to detect every single dynamic memory allocation
by passing "-t 0" parameter to ComDetective, the profiled program can run slowly.  

<br>
<br>

The reason for this is that if the profiled program calls dynamic memory allocation functions 
a lot of times (like millions of mallocs), ComDetective can be slowed down 
as it intercepts these function calls and inserts info on the allocated memory ranges
to its database of dynamic objects. To get around this problem, ComDetective's user can accelerate
ComDetective by restricting it to detect only large objects. 
For example, instead of capturing every single dynamic memory allocation 
(with command line parameter "-t 0"), ComDetective can be restricted to capture only dynamic memory allocations 
with size 10000 bytes or higher (with command line parameter "-t 10000").
