mkdir -p /opt/llvm2kittel/build
cd /opt/llvm2kittel/build
CC=gcc-4.8 CXX=g++-4.8 cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/llvm2kittel/build ../
make -j $(nproc)