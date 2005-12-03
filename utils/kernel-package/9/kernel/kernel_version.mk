# The purpose of this snippet of makefile is to easily and accurately
# extract out the kernel version information.


MAKEFLAGS:=$(filter-out -w,$(MAKEFLAGS))
MFLAGS:=$(filter-out -w,$(FLAGS))

# Include the kernel makefile
override dot-config := 0
include Makefile
dot-config := 0

.PHONY: debian_VERSION debian_PATCHLEVEL debian_SUBLEVEL
.PHONY: debian_EXTRAVERSION debian_LOCALVERSION debian_TOPDIR


debian_VERSION:
	@echo "$(strip $(VERSION))"

debian_PATCHLEVEL:
	@echo "$(strip $(PATCHLEVEL))"

debian_SUBLEVEL:
	@echo "$(strip $(SUBLEVEL))"

debian_EXTRAVERSION:
	@echo "$(strip $(EXTRAVERSION))"

debian_LOCALVERSION:
	@echo "$(strip $(LOCALVERSION))"

debian_TOPDIR:
# 2.6 kernels declared TOPDIR obsolete, so use srctree if it exists
	@echo $(if $(strip $(srctree)),"$(srctree)","$(TOPDIR)")


debian_conf_var:
        @echo "ARCH             = $(ARCH)"
        @echo "HOSTCC           = $(HOSTCC)"
        @echo "HOSTCFLAGS       = $(HOSTCFLAGS)"
        @echo "CROSS_COMPILE    = $(CROSS_COMPILE)"
        @echo "AS               = $(AS)"
        @echo "LD               = $(LD)"
        @echo "CC               = $(CC)"
        @echo "CPP              = $(CPP)"
        @echo "AR               = $(AR)"
        @echo "NM               = $(NM)"
        @echo "STRIP            = $(STRIP)"
        @echo "OBJCOPY          = $(OBJCOPY)"
        @echo "OBJDUMP          = $(OBJDUMP)"
        @echo "MAKE             = $(MAKE)"
        @echo "GENKSYMS         = $(GENKSYMS)"
        @echo "CFLAGS           = $(CFLAGS)"
        @echo "AFLAGS           = $(AFLAGS)"
        @echo "MODFLAGS         = $(MODFLAGS)"


# arch-tag: ecfa9843-6306-470e-8ab9-2dfca1d40613

#Local Variables:
#mode: makefile
#End:
