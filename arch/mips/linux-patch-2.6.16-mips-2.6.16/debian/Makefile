#
# This Makefile is run from the debian subdirectory.
#
# Copyright (C) 2005 Thiemo Seufer
#

#
# Mips tools rules.
#
MT_CFLAGS := -W -Wall -g -pipe
ifneq (,$(findstring noopt,$(DEB_BUILD_OPTIONS)))
 MT_CFLAGS += -O0
else
 MT_CFLAGS += -O2
endif

MT_INSTALL := install -m 755 -o root -g root
ifeq (,$(findstring nostrip,$(DEB_BUILD_OPTIONS)))
  MT_INSTALL += -s
endif

build-mips-tools: bdir = $(CURDIR)/build-mips-tools
build-mips-tools: ../stamps/build-mips-tools
../stamps/build-mips-tools:
	@rm -rf $(bdir)
	@cp -al $(kdir) $(bdir)

	@echo "Building mips tools"
	@cd $(bdir) && make-kpkg clean && make-kpkg debian
	@cd $(bdir)/arch/mips/boot \
		&& $(CC) $(MT_CFLAGS) -o elf2ecoff elf2ecoff.c

	@touch $@

install-mips-tools: build-mips-tools
	$(MT_INSTALL) -D $(kdir)/arch/mips/boot/elf2ecoff debian/$(mtdir)/usr/bin/elf2ecoff

clean-mips-tools:
	rm -rf build-mips-tools

.PHONY: build-mips-tools install-mips-tools clean-mips-tools

#
# Generic mips header package rules.
#
build-generic-headers: bdir = $(CURDIR)/build-generic-headers
build-generic-headers: ../stamps/build-generic-headers
../stamps/build-generic-headers:
	@rm -rf $(bdir)
	@cp -al $(kdir) $(bdir)

	@echo "Building common headers"
	@cd $(bdir) && make-kpkg clean && make-kpkg debian
	# Random config to satisfy make-kpkg
	@cp config/$(debarch)/$(shell ls config/$(debarch) |head -1) $(bdir)/.config
	@cp -p changelog control copyright post-install $(bdir)/debian/
	@echo official > $(bdir)/debian/official
	@cd $(bdir) && HEADER_CLEAN_HOOK='$(CURDIR)/header-generic-hook' \
		bdir=$(bdir) \
		make-kpkg \
		--append-to-version=-$(abiver) \
		--stem linux kernel_headers

	@touch $@

install-generic-headers:

clean-generic-headers:
	rm -f ../stamps/build-generic-headers
	rm -rf build-generic-headers

.PHONY: install-generic-headers clean-generic-headers

#
# Per-flavour rules.
#
build-flavour: bdir = $(CURDIR)/build-flavour-$(flavour)
build-flavour: ../stamps/build-flavour-$(flavour)
../stamps/build-flavour-$(flavour):
	@rm -rf $(bdir)
	@cp -al $(kdir) $(bdir)

	@echo "Building image and headers for $(flavour)"
	@cd $(bdir) && make-kpkg clean && make-kpkg debian
	@cp config/$(debarch)/$(flavour) $(bdir)/.config
	@cp -p changelog control copyright post-install $(bdir)/debian/
	@echo official > $(bdir)/debian/official
	@cd $(bdir) && CONCURRENCY_LEVEL=$(NUM_CPUS) \
		IMAGE_CLEAN_HOOK='$(CURDIR)/image-hook' \
		HEADER_CLEAN_HOOK='$(CURDIR)/header-flavour-hook' \
		bdir=$(bdir) \
		make-kpkg \
		--subarch $(flavour) \
		--append-to-version=-$(abiver)-$(flavour) \
		--stem linux --initrd binary-arch

	touch $@


install-flavour:

clean-flavour:
	rm -f ../stamps/build-flavour-$(flavour)
	rm -rf build-flavour-$(flavour)

.PHONY: build-flavour install-flavour clean-flavour
