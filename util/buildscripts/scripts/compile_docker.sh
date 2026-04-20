#!/bin/bash
set -e
set -x

rm -rf /io/*

cd /
mkdir riderduck_build
cd riderduck_build
CC=clang CXX=clang++ CFLAGS="-fPIC -fvisibility=hidden" cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/io/dist/ -DVULKAN_LAYER_FOLDER=/io/dist/etc/vulkan/implicit_layer.d -DSTATIC_QRIDERDUCK=ON -DQRIDERDUCK_NO_CXX11_REGEX=ON /riderduck
make -j8
make install

# Copy python modules
mkdir /io/pymodules
cp -R lib/*.so /io/pymodules

# Copy python lib folder, and trim
mkdir -p /io/dist/share/riderduck/pylibs/lib
cd /io/dist/share/riderduck/pylibs/lib
cp -R /usr/lib/python3.6/ .
cd python3.6
# remove cache files
rm -rf $(find -iname __pycache__)
# remove unwanted modules
rm -rf test site-packages ensurepip distutils idlelib config-*
