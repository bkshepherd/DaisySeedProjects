#!/bin/bash

START_DIR=$PWD
LIBDAISY_DIR=$START_DIR/dependencies/libDaisy
DAISYSP_DIR=$START_DIR/dependencies/DaisySP
CLOUDSEED_DIR=$START_DIR/dependencies/CloudSeed

echo "Building libDaisy..."
cd "$LIBDAISY_DIR"
make clean
make -j 4
if [ $? -ne 0 ]; then
        echo "Failed to compile libDaisy"
        exit 1
fi
echo "done."

echo "Building DaisySP..."
cd "$DAISYSP_DIR"
make clean
make -j 4
if [ $? -ne 0 ]; then
        echo "Failed to compile DaisySP"
        exit 1
fi
echo "done."

echo "Building Cloudseed..."
cd "$CLOUDSEED_DIR"
make clean
make -j 4
if [ $? -ne 0 ]; then
        echo "Failed to compile Cloudseed"
        exit 1
fi
echo "done."
