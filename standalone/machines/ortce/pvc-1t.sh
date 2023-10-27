#!/bin/bash

module purge
module load cmake
module load intel/oneapi/nightly
module laod intel-comp-rt
module load intel/mpich
module load intel_compute_runtime/release/agama-devel-647

export YAKL_ARCH=SYCL
unset CXXFLAGS
unset FFLAGS
unset F77FLAGS
unset F90FLAGS

export CC=mpicc
export CXX=mpic++
export FC=mpif90

export GATOR_DISABLE=0
export ZE_AFFINITY_MASK=0.0
export SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1

export YAKL_SYCL_FLAGS="-DYAKL_PROFILE -DHAVE_MPI -O3 -fsycl -sycl-std=2020 -fsycl-unnamed-lambda -fsycl-device-code-split=per_kernel -fsycl-targets=spir64_gen -Xsycl-target-backend \"-device 12.60.7 -options -ze-opt-large-register-file\" -mlong-double-64 -Xclang -mlong-double-64 -I`nc-config --includedir`"
export YAKL_F90_FLAGS="-O2 -DSCREAM_DOUBLE_PRECISION"
export PAM_LINK_FLAGS="-L`nc-config --libdir` -lnetcdf"
export SCREAM_HOME="$HOME/scream"
export YAKL_HOME="$HOME/YAKL"
export PAM_SCREAM_CXX_FLAGS="-O3;-DHAVE_MPI;-I$YAKL_HOME/external"
export SCREAM_Fortran_FLAGS="-O3"
export SCREAM_CXX_LIBS_DIR="`pwd`/../../mmf_simplified/build_p3_shoc_cxx"
export PAM_NLEV=50
export PAM_SCREAM_USE_CXX="ON"
