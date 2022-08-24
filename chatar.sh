#!/bin/sh
# (C)2018 Remo Andreoli

#set -x

# CONFIG
name=`basename $0`

# HELPER

usage()
{
    cat <<EOF
Usage: $name [-help] conffile t
Salva i file piu' vecchi d it minuti in DirName.tar.gz
Note:
      
       1) if t=0, stampa a schermo dei file
       2) DirName si trova nei file di config
       3) DirName rimossa, se il tar viene creato con successo

EOF
exit
}

# SYNTAX

[ $# -lt 2 ] && usage
case $1 in
    -help)
	usage;;
esac

#------------
# MAIN
#------------

conf=/tmp/conf.$$; t=$2
cat $1 |  tr -d ' \t' > $conf
# include il file di conig nello script namespace
. $conf

if [ ! -d $DirName ] || [ $DirName == "/" ] ; then
    echo "[$DirName]  NON ESISTE"; exit
fi

echo "DirName=$DirName, t=$t"

# PRINT?
if [ $t -eq 0 ]; then
    echo "== list of $DirName ==="
    /bin/ls -1q $DirName
    echo "======="
    exit
fi


# TAR

tgz="`basename $DirName`.tar.gz"

# lista dei file

list=`find $DirName -mmin +$t -type f`
#echo "list=[$list]"
[ -z "$list" ] && echo "Niente da fare" && exit

if tar zcfP $tgz $list ; then
    #tar -ztf $tgz
    echo "Risultato in [$tgz]."
    [ -d "$DirName" ] && echo "Rimuovo dir [$DirName] ... " && rm -r  $DirName
else
    echo "ERRORE, TAR NON CREATO"
fi
    

# End
