#!/bin/bash

./cmakeclean.sh

cmake    \
  -DPAM_SCREAM_CXX_FLAGS="${PAM_SCREAM_CXX_FLAGS}"                \
  -DYAKL_ARCH="${YAKL_ARCH}"                                      \
  -DKokkos_ARCH_AMPERE80="${Kokkos_ARCH_AMPERE80}"                \
  -DKokkos_ARCH_HOPPER90="${Kokkos_ARCH_HOPPER90}"                \
  -DYAKL_HOME="${YAKL_HOME}"                                      \
  -DSCREAM_HOME="${SCREAM_HOME}"                                  \
  -DSCREAM_DOUBLE_PRECISION=ON                                    \
  -DCMAKE_CUDA_HOST_COMPILER="`which mpic++`"                     \
  -DPAM_NLEV=${PAM_NLEV}                                          \
  -DMACH=sunspot                                                  \
  -DCMAKE_VERBOSE_MAKEFILE=ON                                     \
  -DPAM_STANDALONE=ON                                             \
  ../../../physics/scream_cxx_interfaces

