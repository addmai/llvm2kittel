FROM ubuntu:18.04 AS builder

# Install dependencies for llvm and llvm2kittel
RUN apt update && apt install -y \
        build-essential \
        git \
        gdb \
        wget \
        xz-utils \
        gcc-4.8 \
        g++-4.8 \
        cmake \
        python \
        groff \
        libgmp-dev
RUN apt clean \
        && rm -rf /var/lib/apt/lists/*
WORKDIR /opt

# Build & install llvm from source
RUN wget -O - https://releases.llvm.org/3.6.2/llvm-3.6.2.src.tar.xz | tar Jxf -
RUN wget -O - https://releases.llvm.org/3.6.2/cfe-3.6.2.src.tar.xz | tar Jxf - -C /opt/llvm-3.6.2.src/tools
RUN mv /opt/llvm-3.6.2.src/tools/cfe-3.6.2.src /opt/llvm-3.6.2.src/tools/clang
# RUN git clone https://github.com/gyggg/llvm2kittel.git -b kou

# Build & install llvm
WORKDIR /opt/llvm-3.6.2.src
RUN mkdir -p build
WORKDIR /opt/llvm-3.6.2.src/build
RUN CC=gcc-4.8 CXX=g++-4.8 CFLAGS="-g" CXXFLAGS="-g" ../configure --enable-optimized
RUN make -j20
RUN make install

# 実行用・開発用で共通して同じパスでビルドを行う
ENV PATH="/opt/llvm2kittel/build/:$PATH"


# 実行用、コンテナ構築時にソースコードの転送とビルドを行う
FROM builder AS runtime
# Build llvm2kittel
COPY . /opt/llvm2kittel

RUN mkdir -p /opt/llvm2kittel/build
WORKDIR /opt/llvm2kittel/build

RUN CC=gcc-4.8 CXX=g++-4.8 cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/llvm2kittel/build ../
RUN make -j20

WORKDIR /root


# 開発用、コンテナ構築後にソースコードをbind mountしてビルドする
# テストデータとしてベンチマーク問題をコンテナ上にダウンロードしておく
FROM builder AS dev

RUN git clone https://github.com/TermCOMP/TPDB.git
RUN ln -s /opt/llvm2kittel/test /root/
RUN ln -s /root/TPDB/C /root/benchmarks

WORKDIR /opt/llvm2kittel/build