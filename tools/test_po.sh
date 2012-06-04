#!/bin/bash
cd ..
if [ `id -u` != 0 ]; then exit 1; fi
if [ "$1" == "" ]; then exit 1; fi
msgfmt --check po/${1}.po
msgfmt --statistics po/${1}.po
msgfmt -o /usr/share/locale/${1}/LC_MESSAGES/e4rat-lite.mo po/${1}.po
exit 0
