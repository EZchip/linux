
targets := vmlinux.bin vmlinux.bin.gz uImage

MKIMAGE := $(srctree)/scripts/mkuboot.sh

OBJCOPYFLAGS= -O binary -R .note -R .note.gnu.build-id -R .comment -S

LINUX_START_TEXT = $$(readelf -h $(srctree)/vmlinux | grep "Entry point address" | grep -o 0x.*)

quiet_cmd_uimage = UIMAGE  $@
	cmd_uimage = $(CONFIG_SHELL) $(MKIMAGE) -A arc -O linux -T kernel	\
		-C gzip -a $(CONFIG_LINUX_LINK_BASE) -e $(LINUX_START_TEXT) -d $< $@ 

$(obj)/vmlinux.bin: vmlinux FORCE
	$(call if_changed,objcopy)
		
$(obj)/vmlinux.bin.gz: $(obj)/vmlinux.bin FORCE
	$(call if_changed,gzip)

$(obj)/uImage: $(obj)/vmlinux.bin.gz FORCE
	$(call if_changed,uimage)

PHONY += FORCE

