Using ReuseTracker
==================
To run perf_event_open system call without having to use sudo access,
set the value of perf_event_paranoid to -1 by typing the following command:
sudo sysctl -w kernel.perf_event_paranoid=-1

<ol type="1">
<li> Compile the code that you want to profile using "-g" flag to allow for debugging.</li>

<li> To run ReuseTracker to profile reuse distance in private caches: 

HPCRUN_WP_REUSE_PROFILE_TYPE=\<"TEMPORAL"\|"SPATIAL"\> HPCRUN_PROFILE_L3=false HPCRUN_WP_CACHELINE_INVALIDATION=true HPCRUN_WP_DONT_FIX_IP=true HPCRUN_WP_DONT_DISASSEMBLE_TRIGGER_ADDRESS=true hpcrun -o \<name of output folder\> -e WP_REUSETRACKER -e MEM_UOPS_RETIRED:ALL_LOADS@\<sampling period\> -e MEM_UOPS_RETIRED:ALL_STORES@\<sampling period\> <./your_executable> your_args
</li>
</ol>



