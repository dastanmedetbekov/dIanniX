#!/bin/bash

BUILD_DIR="build"

if [ "$1" = "clean" ]; then
    rm -rf $BUILD_DIR
    make clean 2>/dev/null
    rm -f Makefile* .qmake.stash iannix IanniX
    echo "Cleaned"
    exit 0
fi

if [ "$1" = "cmake" ]; then
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    cmake ..
    make -j$(nproc)
    echo "Built with CMake in $BUILD_DIR/"
    exit 0
fi

if [ "$1" = "qt6" ]; then
    echo "Building with Qt6 (CMake)..."
    mkdir -p $BUILD_DIR
    cd $BUILD_DIR
    cmake ..
    make -j$(nproc)
    echo "Built with Qt6 in $BUILD_DIR/"
    exit 0
fi

mkdir -p $BUILD_DIR
qmake IanniX.pro
make -j$(nproc)
echo "Built with qmake (objects in $BUILD_DIR/)"
