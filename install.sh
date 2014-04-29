#!/bin/bash
set -e # stop on error
. common.sh

# install driver source code
if [ ! -d "$SRC" ]; then
	echo "Installing source to: $SRC"
	# extract files into /usr/src
	git archive --prefix="$TAG/" HEAD | sudo tar -x -C /usr/src
	# write DKMS config
	cat >"$SRC/dkms.conf" <<-END
		PACKAGE_NAME=$NAME
		PACKAGE_VERSION=$VERSION
		MAKE[0]="make KERNEL_VERSION=\${kernelver} MIC_CARD_ARCH=$ARCH"
		CLEAN=true
		BUILT_MODULE_NAME[0]=mic
		DEST_MODULE_LOCATION[0]=/extra
		AUTOINSTALL=yes
	END
fi

dkms add $NAME/$VERSION
dkms install $NAME/$VERSION

# install additional udev rules
if [ $CONFIG -eq 1 ]; then
	echo "Installing configuration into: $PREFIX/etc"
	make MIC_CARD_ARCH=$ARCH prefix=$PREFIX conf_install
fi
