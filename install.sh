#! /bin/bash -e
. common.sh

# install driver source code
if [ ! -d "$SRC" ]; then
	echo "Installing source to: $SRC"
	# extract files into /usr/src
	git archive --prefix="$TAG/" HEAD | tar -x -C /usr/src/
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
if ((INSTALL_CONFIG)); then
	echo "Installing configuration into: $SYSCONFDIR"
	make MIC_CARD_ARCH=$ARCH sysconfdir=$SYSCONFDIR conf_install
fi

# install additional developer header files
if ((INSTALL_HEADER)); then
	echo "Installing developer header files into: $INCLUDEDIR"
	make MIC_CARD_ARCH=$ARCH includedir=$INCLUDEDIR dev_install
fi
