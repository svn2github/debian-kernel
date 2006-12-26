#make parameters

SINGLE_PRECISION=1
DOUBLE_PRECISION=1

CC=gcc
LD=ld 

CFLAGS = -O2 -Wstrict-prototypes -fomit-frame-pointer -fno-strict-aliasing -pipe -fno-strength-reduce -Wall -Wno-comment

OBJS :=
ifdef SINGLE_PRECISION
OBJS += e_powf.o e_log10f.o e_logf.o e_sqrtf.o s_ceilf.o s_floorf.o s_isinf.o 
OBJS += s_fabsf.o
OBJS += s_scalbnf.o s_copysignf.o s_truncf.o s_sinf.o k_sinf.o k_cosf.o e_rem_pio2f.o k_rem_pio2f.o
endif
ifdef DOUBLE_PRECISION
OBJS += e_pow.o e_log10.o e_log.o e_sqrt.o s_ceil.o s_floor.o 
OBJS += s_fabs.o
OBJS += s_scalbn.o s_copysign.o s_trunc.o s_sin.o k_sin.o k_cos.o e_rem_pio2.o k_rem_pio2.o
endif

modules: libm

libm: $(OBJS) libm.a

%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $<

libm.a: $(OBJS)
	$(LD) -r -o libm.a $(OBJS)

modules_install: modules

clean:
	/bin/rm -f *~
	/bin/rm -f *.s
	/bin/rm -f *.o
	/bin/rm -f *.a
