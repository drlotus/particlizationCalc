#!/bin/bash
export CPATH=/opt/homebrew/Cellar/libomp/16.0.2/include/
export LIBRARY_PATH=/opt/homebrew/Cellar/libomp/16.0.2/lib  # Specify the path to the libomp library
export CC=/opt/homebrew/Cellar/llvm/16.0.6/bin/clang  # Specify the Clang compiler from LLVM
export CXX=/opt/homebrew/Cellar/llvm/16.0.6/bin/clang++
make clean && make
