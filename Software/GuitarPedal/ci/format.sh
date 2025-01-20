#!/bin/bash

# From /Software/GuitarPedal/ you can run ./ci/format.sh to format all the code in the project
START_DIR=$PWD
find $START_DIR/Effect-Modules/ -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' -exec clang-format -style=file -i {} \;
find $START_DIR/Hardware-Modules/ -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' -exec clang-format -style=file -i {} \;
find $START_DIR/UI/ -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' -exec clang-format -style=file -i {} \;
find $START_DIR/Util/ -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' -exec clang-format -style=file -i {} \;
clang-format -style=file -i $START_DIR/*.cpp
clang-format -style=file -i $START_DIR/*.h
