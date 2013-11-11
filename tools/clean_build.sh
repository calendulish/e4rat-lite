#!/bin/sh
cd ..
rm -fv *.a
rm -fv e4rat-lite-*
rm -fv libe4rat-lite-*
rm -rfv build
potd=(`find doc/ -type d -exec basename {} \;`)
for ((i=1;i<${#potd[@]};i++)); do
	 rm -fv doc/${potd[$i]}/*.8
	 rm -fv doc/${potd[$i]}/*.5
done
exit 0
