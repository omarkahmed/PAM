# PAM: Portable Atmospheric Model

## Directory Structure

* `build`: Machine files for compiling
* `common`: Source & header files with functions and variables used in multiple places
* `driver`: Code to drive the standalone model
* `dynamics`: Dynamical core code, each version in its own sub-directory. Each dycore option has its own `math_desc` files to document the numerical discretizations used.
* `externals`: Submodules of external code (`cub`, `hipcub`, `rocPRIM`, `kokkos`, `YAKL`)
* `physics`: Physics parameterizations (microphysics, radiation, etc.)
* `sage`: SageMath code for generating C++ code
* `utils`: Miscillaneous utilities

## Software Dependencies
* MPI library
* parallel-netcdf (https://trac.mcs.anl.gov/projects/parallel-netcdf)
