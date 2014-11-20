#!/bin/sh
cd ..
if [ "$1" == "" ]; then
    pofile=e4rat-lite
    sufix=pot
    if [ -f ${pofile}.${sufix} ]; then
        rm po/${pofile}.${sufix}
    fi
else
    pofile=${1}
    sufix=po
fi

_basearray=($(find src/ -type f \
            \( -name "*.cc" -o -name "*.c" \) \
            -exec basename {} \;))

if [ -e po/${pofile}.${sufix}.bkp ]; then
    echo "Apagando backup antigo de po/${pofile}.${sufix}"
    rm -f po/${pofile}.${sufix}.bkp
fi

if [ -f po/${pofile}.${sufix} ]; then
echo "Criando backup de po/${pofile}.${sufix} para po/${pofile}.${sufix}.bkp"
    cp po/${pofile}.${sufix} po/${pofile}.${sufix}.bkp
fi
            
function create() {
echo "Criando po/${pofile}.${sufix} a partir de src/$1"
xgettext -d e4rat-lite -o po/${pofile}.${sufix} -k_ -s src/$1
sed --in-place po/${pofile}.${sufix} --expression=s/CHARSET/UTF-8/
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
            create $file
        fi
    fi
done

