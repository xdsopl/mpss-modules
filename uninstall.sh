#! /bin/bash -e
. common.sh

# remove DKMS module
dkms remove $NAME/$VERSION --all

# remove source package
if [ -d "$SRC" ]; then
	echo "Removing source from: $SRC"
	rm -rf "$SRC"
fi

echo "Not removing config files in $SYSCONFDIR"
echo "Not removing developer header files in $INCLUDEDIR"

