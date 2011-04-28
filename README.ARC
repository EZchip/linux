How to build the ARC linux kernel to a known working configuration.

Select either arch/arc/configs/iss_defconfig or arch/arc/configs/aa4_defconfig
depending on whether you are using the MetaWare simulator or the ARC Angel 4
hardware.

Copy the relevant file to .config and build the kernel.

$ cp arch/arc/config/aa4_defconfig .config
$ make oldconfig
$ make clean
$ make bootpImage


# Note : On a certain Ubuntu machine here in the ARC labs we have seen that the first time after a "make clean" the "make bootpImage" fails.  If you see this, don't worry, simply do another make clean and make bootpImage and all will be fine.
