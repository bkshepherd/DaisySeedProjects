#!/bin/bash
echo "building GuitarPedal firmware..."
echo "arg: $1"
make clean
make -j 4 $1
if [ $? -ne 0 ]; then
        echo "Failed to compile GuitarPedal firmware"
        exit 1
fi
echo "done."
