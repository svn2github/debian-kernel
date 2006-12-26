#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
#	include <linux/config.h>
#endif
#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/types.h>
#include <stdio.h>
#include <math.h>
#include <bits/nan.h>
#include <bits/errno.h>

//======================================================================
// Floating Point support
// This is needed when using floating point in linux modules linked
// with libm.a.
//======================================================================
FILE *stderr = NULL; // The floating point library needs this when error occurs
int errno=0;
extern void __assert_fail (const char *__assertion, const char *__file,
			   unsigned int __line, __const char *__function);


extern int __isnan(double f)
{
	return f == NAN;
}

extern int fputs (const char *s, FILE *stream)
{
	WARN("%s",s);
	return 0;
}

extern size_t fwrite (const void *ptr, size_t size,size_t n, FILE *s)
{
	WARN("\n");
	return n;
}

extern int *__errno_location (void) 
{
	return &errno;
}

extern void __assert_fail (const char *__assertion, const char *__file,
			   unsigned int __line, __const char *__function)
{
     printk(KERN_ERR "%s %s LINE%d %s\n",__assertion,__file,__line,__function);
     BUG();
}
