#!/usr/bin/env bash
set -x
set -e
# Need SWIG >= 4.0.0
cd /tmp/ &&
wget https://github.com/swig/swig/archive/rel-4.0.0.tar.gz &&
tar zxf rel-4.0.0.tar.gz && cd swig-rel-4.0.0 &&
./autogen.sh && ./configure --prefix "${HOME}"/swig/ 1>/dev/null &&
make >/dev/null &&
make install >/dev/null;

export DOXYGEN_VERSION=1.8.15
export DOXYGEN_VER=doxygen-${DOXYGEN_VERSION}
export DOXYGEN_TAR=${DOXYGEN_VER}.linux.bin.tar.gz
export DOXYGEN_URL="https://sourceforge.net/projects/doxygen/files/rel-${DOXYGEN_VERSION}/${DOXYGEN_TAR}/download"
wget -O - "${DOXYGEN_URL}" | tar xz -C ${HOME} ${DOXYGEN_VER}/bin/doxygen &&
mkdir -p ${HOME}/bin && mv ${HOME}/${DOXYGEN_VER}/bin/* ${HOME}/bin

export CMAKE_VERSION=3.14.3
export CMAKE_VER=cmake-${CMAKE_VERSION}
export CMAKE_TAR=${CMAKE_VER}-Linux-x86_64.tar.gz
export CMAKE_URL="https://cmake.org/files/v3.14/${CMAKE_TAR}"
mkdir -p ${HOME}/cmake && cd ${HOME}/cmake && wget -O - "${CMAKE_URL}" | tar -xzf - --strip-components=1
