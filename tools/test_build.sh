#!/bin/sh
cd ..
if [ "$1" == "" ]; then type=debug; else type=$1; fi
if [ -e build ]; then rm -rfv build; fi
mkdir build && cd build
echo "---~---"
cmake .. -DCMAKE_BUILD_TYPE=$type -DCMAKE_INSTALL_PREFIX=""
echo "---~---"
make -j5
echo "---~---"
mkdir test
make DESTDIR="test" install
echo "---~---"
ls test/
echo "---~---"
ls
echo "---~---"

exit 0
