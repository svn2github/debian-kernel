GCC	:= gcc -m32
CFLAGS	:= -Wall -Wstrict-prototypes -O2 -g -fomit-frame-pointer -fno-builtin \
	   -nostdinc -isystem $(shell $(GCC) -print-file-name=include) -fPIC
AFLAGS	:= -D__ASSEMBLY__ $(CFLAGS) -traditional -nostdinc

src := crt0.S string.S prom.c stdio.c main.c div64.S
src += infblock.c infcodes.c inffast.c inflate.c inftrees.c infutil.c
src += mkvmlinuz-kernel-initrd.c mkvmlinuz-kernel-vmlinux.strip.c
obj := $(addsuffix .o, $(basename $(src)))
 
progs := addnote

all: $(obj) $(progs)

$(patsubst %.c,%.o, $(filter %.c, $(src))): %.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

$(patsubst %.S,%.o, $(filter %.S, $(src))): %.o: %.S
	$(GCC) $(AFLAGS) -c -o $@ $<

mkvmlinuz-kernel-initrd.c mkvmlinuz-kernel-vmlinux.strip.c:
	touch $@

addnote: addnote.c
	$(GCC) -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -o addnote addnote.c

clean:
	rm -f $(obj) mkvmlinuz-kernel-initrd.c mkvmlinuz-kernel-vmlinux.strip.c $(progs)
