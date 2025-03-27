# ParCoreTools

ParCoreTools is a suite of profiling tools and benchmarks developed by [ParCoreLab](https://parcorelab.ku.edu.tr/) at Koç University, Istanbul.

The profiling tools included in ParCoreTools are as follows.

- [ComDetective](docs/ComDetective.md): Inter-thread communication analyzer for shared memory parallel programs 

Published in M. A. Sasongko, P. Akhtar, M. Chabbi, D. Unat, "[ComDetective: A Lightweight Communication Detection Tool for Threads](https://dl.acm.org/doi/10.1145/3295500.3356214)", 2019 IEEE/ACM Supercomputing Conference, Denver, Colorado, USA, November 2019.
Its accuracy verification benchmarks can be found [here](https://github.com/ParCoreLab/accuracy-verification-microbenchmarks).

- [ReuseTracker](docs/ReuseTracker.md): Reuse distance analyzer for multithreaded code

Published in M. A. Sasongko, M. Chabbi, M. B. Marzijarani, and D. Unat. "[ReuseTracker: Fast Yet Accurate Multicore Reuse Distance Analyzer](https://dl.acm.org/doi/10.1145/3484199)", ACM Trans. Arch. Code Optim, Vol. 19, Issue 1, Article 3, December 2021.
Information on how to reproduce the results reported on the paper, and the used accuracy verification benchmarks can be found [here](https://github.com/ParCoreLab/ReuseTracker).

- [ComScribe](https://github.com/ParCoreLab/ComScribe/): Inter-GPU Communication Analyzer for Multi-GPU Applications

Published in: 

1. P. Akhtar, E. Tezcan, F. M. Qararyah, and D. Unat. "[ComScribe: Identifying Intra-node GPU Communication", 2020 International Symposium on Benchmarking, Measuring and Optimization](https://link.springer.com/chapter/10.1007/978-3-030-71058-3_10), November 2020.

2. M. A. Soyturk, P. Akhtar, E. Tezcan, and D. Unat. "[Monitoring Collective Communication Among GPUs](https://arxiv.org/abs/2110.10401)", CoRR abs/2110.10401, 2021


The benchmarks included in ParCoreTools are as follows.

- [pes-benchs](https://github.com/ParCoreLab/PES-artifact): A benchmark suite that evaluates precise event sampling features.

- [comdetective-benchs](https://github.com/ParCoreLab/accuracy-verification-microbenchmarks): A benchmark suite that evaluates the accuracy of inter-thread communication analyzers.

- [reuse-benchs](https://github.com/ParCoreLab/ReuseTracker/tree/master/reuse-invalidation-benchs): A benchmark suite that evaluates the accuracy of reuse distance analyzers.

Published in 

1. M. A. Sasongko, M. Chabbi, P. H. J. Kelly and D. Unat, "Precise Event Sampling on AMD Versus Intel: Quantitative and Qualitative Comparison," in IEEE Transactions on Parallel and Distributed Systems, vol. 34, no. 5, pp. 1594-1608, May 2023, https://doi.org/10.1109/TPDS.2023.3257105.       

2. Sasongko MA, Chabbi M, Kelly PHJ, Unat D. Precise event sampling-based data locality tools for AMD multicore architectures. Concurrency Computat Pract Exper. 2023; 35(24):e7707. https://doi.org/10.1002/cpe.7707

These works are supported by:

1. The Scientific and Technological Research Council of Turkey (TUBITAK), Grant no. 215E193
2. The Scientific and Technological Research Council of Turkey (TUBITAK), Grant no. 120E492
3. The Royal Society-Newton Advanced Fellowship NAF\R2\202207.
