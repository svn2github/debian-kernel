#ifndef _LINUX_KERNEL_H
#include <linux/kernel.h>
#endif

#define ATM_D 1
#define DATA_D 2
#define RW_D 4
#define PCI_D 8
#define USB_D 8
#define TOSCA_D 16
#define INTR_D 32
#define RAPI_D 64

#ifdef _PCI_DRIVER
#define LOG_PREFIX "unicorn_pci"
#endif
#ifdef _USB_DRIVER
#define LOG_PREFIX "unicorn_usb"
#endif
#ifdef _ATM_DRIVER
#define LOG_PREFIX "unicorn_atm"
#endif
#ifdef _ETH_DRIVER
#define LOG_PREFIX "unicorn_eth"
#endif

#ifndef LOG_PREFIX
#define LOG_PREFIX ""
#endif

extern unsigned long DebugLevel;

#define INFO(fmt,arg...) \
	printk( KERN_INFO LOG_PREFIX ": " fmt,## arg) 

#define WARN(fmt,arg...) \
	do { printk(KERN_WARNING  "%s: ",__FUNCTION__); \
	printk(fmt,## arg); } while(0); 

#define ERR(fmt,arg...) \
	do { printk(KERN_ERR  "%s: ",__FUNCTION__); \
	printk(fmt,## arg); } while(0); 

#if DEBUG
#define DBG(level,fmt,arg...) \
if (level & DebugLevel) { \
	printk(KERN_DEBUG  "%s: ",__FUNCTION__); \
	printk(fmt,## arg); }

#undef ASSERT
#define ASSERT(test) ((test) ? (void)0 : printk(KERN_ERR "%s : %s: %s",__FILE__,__FUNCTION__,#test))

static void __attribute__((unused))
dump_packet(const char *name,const u_char *data,int pkt_len)
{
#define DUMP_HDR_SIZE 20
#define DUMP_TLR_SIZE 8
	if (pkt_len) {
		int i,len1,len2;

		printk(KERN_DEBUG "%s: length=%d,data=",name,pkt_len);

		if (pkt_len >  DUMP_HDR_SIZE+ DUMP_TLR_SIZE) {
			len1 = DUMP_HDR_SIZE;
			len2 = DUMP_TLR_SIZE;
		} else {
			len1 = pkt_len > DUMP_HDR_SIZE ? DUMP_HDR_SIZE : pkt_len;
			len2 = 0;			
		}
		for (i = 0; i < len1; ++i) {
		 	printk ("%.2x", data[i]);
		}
		if (len2) {
		 	printk ("..");
			for (i = pkt_len-DUMP_TLR_SIZE; i < pkt_len; ++i) {
				printk ("%.2x", data[i]);
			}
		}
		printk ("\n");
	}
#undef DUMP_HDR_SIZE
#undef DUMP_TLR_SIZE
}

#define DUMP_PACKET(level,data,len) \
if (level & DebugLevel) { \
dump_packet(__FUNCTION__,data,len);}


#else

#define DBG(level,format, arg...) do {} while (0)
#undef ASSERT
#define ASSERT(test)
#define DUMP_PACKET(level,data,len)  do {} while (0)

#endif
