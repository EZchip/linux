How to build the ARC linux kernel to a known working configuration.

Select either the arc_config or arc_simulator_config depending on whether you are
using the MetaWare simulator or the ARC Angel 4 hardware.

Copy the relevant file to .config and build the kernel.

$ cp arc_simulator_config .config
$ make oldconfig
$ make clean
$ make bootpImage


# Note : On a certain Ubuntu machine here in the ARC labs we have seen that the first time
after a "make clean" the "make bootpImage" fails.  If you see this, don't worry, simply do
another make clean and make bootpImage and all will be fine.
