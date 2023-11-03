#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

module purge
module load cmake
module load intel/oneapi
module load intel/dpl
module load intel-nightly/20230926
module load intel-nightly/dpct/20230926
module load intel/mkl-nda/xmain-nightly/20230913
#module load intel-comp-rt
module load intel-comp-rt/agama-ci-devel/737
module load -f intel/mpich

export YAKL_ARCH=SYCL
unset CXXFLAGS
unset FFLAGS
unset F77FLAGS
unset F90FLAGS

export CC=mpiicx
export CXX=mpiicpx
export FC=mpiifx

export PATH=/nfs/site/home/omarahme/git-repos/applications.hpc.workloads.e3sm.pam/libs/oneapi-pnetcdf-ifort_mpich/20231018/bin:${PATH}

export LD_LIBRARY_PATH=/nfs/site/home/omarahme/git-repos/applications.hpc.workloads.e3sm.pam/libs/oneapi-pnetcdf-ifort_mpich/20231018/lib:${LD_LIBRARY_PATH}

export GATOR_DISABLE=0
export ZE_AFFINITY_MASK=0.0
export SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS=1

#export YAKL_SYCL_FLAGS="-DYAKL_PROFILE -DHAVE_MPI -O3 -fsycl -sycl-std=2020 -fsycl-unnamed-lambda -fsycl-device-code-split=per_kernel -fsycl-targets=spir64_gen -Xsycl-target-backend \"-device 12.60.7 -options -ze-opt-large-register-file\" -mlong-double-64 -Xclang -mlong-double-64 -I`nc-config --includedir`"
export YAKL_SYCL_FLAGS="-g -DYAKL_PROFILE -DHAVE_MPI -O3 -fsycl -sycl-std=2020 -fsycl-unnamed-lambda -fsycl-device-code-split=per_kernel -fsycl-targets=spir64_gen -Xsycl-target-backend \"-device 12.60.7 -options -ze-opt-large-register-file\" -mlong-double-64 -I`nc-config --includedir`"
export YAKL_F90_FLAGS="-O3 -DSCREAM_DOUBLE_PRECISION"
export PAM_LINK_FLAGS="-L`nc-config --libdir` -lnetcdf"
export SCREAM_HOME="${SCRIPT_DIR}/../../../../../../../../../../"
export YAKL_HOME="${SCREAM_HOME}/externals/YAKL"
export PAM_SCREAM_CXX_FLAGS="-g;-O3;-DHAVE_MPI;-I$YAKL_HOME/external"
export SCREAM_Fortran_FLAGS="-O3"
export SCREAM_CXX_LIBS_DIR="${SCRIPT_DIR}/../../mmf_simplified/build_p3_shoc_cxx"
export PAM_NLEV=50
export PAM_SCREAM_USE_CXX="ON"
