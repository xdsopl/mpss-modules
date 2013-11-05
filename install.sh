#! /bin/bash

KERNEL=$(uname -r)
#KERNEL=3.9.9-ainan
ARCH=k1om

make KERNEL_VERSION=$KERNEL MIC_CARD_ARCH=$ARCH
sudo make KERNEL_VERSION=$KERNEL MIC_CARD_ARCH=$ARCH sysconfdir=/etc includedir=/usr/include install

