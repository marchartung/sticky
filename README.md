# Sticky SAT
Sticky is a parallel SAT solver originally based on [Glucose-Syrup 4.0](https://www.labri.fr/perso/lsimon/glucose/). Sticky uses physical clause sharing, whereby all clauses are represented only once in memory. 
This is an early stage prototype and is in this version not under development any more.

## Build

```
cd sticky
mkdir build
cd build
cmake ..
make -j
```

## Run examples:
Running standard sticky with 4 threads, maximal 1 hour (3600s) runtime and 4 GB (4000MB) memory:
```
./sticky -nthreads=4 -maxmemory=4000 -maxtime=3600 ~/path/to/sat_problem.cnf
```
Running sticky with export vivification and a max LBD of 4 for vivification clauses:
```
./sticky -nthreads=4 -useExportVivi -maxVifiLBD=4 ~/path/to/sat_problem.cnf
```
For more useful flags use one of the following two lines:
```
./sticky --help # shows available flags
./sticky --help-verb # shows flags and short describtions
```
## Comment
Sticky was developed at [Zuse Institute Berlin](https://zib.de). Employees were funded by [BMBF](https://www.bmbf.de/) under grand 01IH15006C.
We thank the Glucose-Syrup team and the Minisat team for providing the baseline source codes and insides into SAT solving.

For questions contact hartung(ät)zib(dot)de
