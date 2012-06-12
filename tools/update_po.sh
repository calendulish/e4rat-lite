#!/bin/sh
cd ..
if [ "$1" == "" ]; then
	pofile=e4rat-lite
	sufix=pot
else
	pofile=${1}
	sufix=po
fi

_basearray=($(find src/ -type f \
            -name "*.cc" \
            -exec basename {} \;))
            
function create() {
echo "Criando po/${pofile}.${sufix} a partir de src/$1"
xgettext -d e4rat-lite -o po/${pofile}.${sufix} -k_ -s src/$1
}

function update() {
echo "Atualizando po/${pofile}.${sufix} a partir de src/$1"
xgettext -d e4rat-lite -o po/${pofile}.${sufix} -k_ -j -s src/$1
}

for file in ${_basearray[@]}; do
	if [ "$2" == "--new" ]; then
		if [ ! -f "po/${pofile}.${sufix}" ]; then
			unset nofirst
		fi
		if [ ! -n "$nofirst" ]; then
			sufix=pot
			create $file
			nofirst=true
		else
			update $file
		fi
	else
		if [ -f "po/${pofile}.${sufix}" ]; then
			update $file
		else
			echo "Arquivo não encontrado."
			exit 1
		fi
	fi
done

