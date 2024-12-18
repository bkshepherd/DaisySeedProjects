#!/bin/bash

START_DIR=$PWD
LIBDAISY_DIR=$PWD/libDaisy
DAISYSP_DIR=$PWD/DaisySP

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
