#!/bin/bash
set -e # stop on error
. common.sh

# remove DKMS module
dkms remove $NAME/$VERSION --all

# remove source package
if [ -d "$SRC" ]; then
	echo "Removing source from: $SRC"
	rm -rf "$SRC"
fi
