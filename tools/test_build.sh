#!/bin/sh
cd ..
if [ "$1" == "" ]; then type=debug; else type=$1; fi
if [ "$2" == "global" ] && [ `id -u` != 0 ]; then exit 1; fi
if [ -e build ]; then rm -rfv build; fi
mkdir build && cd build
echo "---~---"
cmake .. -DCMAKE_BUILD_TYPE=$type -DCMAKE_INSTALL_PREFIX="/usr"
echo "---~---"
make -j5
echo "---~---"
if [ "$2" != "global" ]; then
	mkdir test
	make DESTDIR="test" install
	echo "---~---"
	ls test/
else
	make install
	mandb
fi
echo "---~---"
ls
echo "---~---"

exit 0
