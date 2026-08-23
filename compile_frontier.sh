#!/bin/bash

#set -x
set -e

base_dir=`pwd`

module load git
module load ninja
module load cmake
module load gcc-native/14.2

#cmake_generator="Unix Makefiles"
cmake_generator="Ninja"

export CRAYPE_LINK_TYPE=dynamic

tbb_repo=https://github.com/oneapi-src/oneTBB.git
tbb_branch=v2023.0.0

tbb_src_dir=${base_dir}/tbb/src
tbb_build_dir=${base_dir}/tbb/build
tbb_install_dir=${base_dir}/tbb/install

if true; then
rm -rf "${tbb_src_dir}"
rm -rf "${tbb_build_dir}"
rm -rf "${tbb_install_dir}"
if [[ ! -d ${tbb_src_dir} ]] ; then
  git clone -b ${tbb_branch} ${tbb_repo} "${tbb_src_dir}"
fi
cmake -G "${cmake_generator}" -S "${tbb_src_dir}" -B "${tbb_build_dir}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DTBB_TEST=OFF \
  -DCMAKE_INSTALL_PREFIX="${tbb_install_dir}"

time cmake --build "${tbb_build_dir}" -j
cmake --install "${tbb_build_dir}"
fi

vtkdie_src_dir=${base_dir}
vtkdie_build_dir=${base_dir}/build
vtkdie_install_dir=${base_dir}/install

if true; then
rm -rf "${vtkdie_build_dir}"
rm -rf "${vtkdie_install_dir}"
cmake -G "${cmake_generator}" -S "${vtkdie_src_dir}" -B "${vtkdie_build_dir}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_INSTALL_PREFIX="${vtkdie_install_dir}" \
  -DTBB_ROOT="${tbb_install_dir}"

time cmake --build "${vtkdie_build_dir}" -j
cmake --install "${vtkdie_build_dir}"
fi
