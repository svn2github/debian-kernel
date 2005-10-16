/* trymount.c: try activating interfaces and mount a future root

    Copyright (C) 2005, Erik van Konijnenburg

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

*/
#include <alloca.h>
#include <stdio.h>
#include <stdarg.h>		/* for va_args */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>		/* for getopt */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <config.h>
#include "ipconfig/netdev.h"

/*
 * ipconfig returns linked list of working devices here.
 */
extern int ipconfig_main (int argc, char *argv[]);
extern struct netdev *ifaces;

/* arguably, this should be in netdev.h */
static inline const char *my_inet_ntoa(uint32_t addr)
{
	struct in_addr a;

	a.s_addr = addr;

	return inet_ntoa(a);
}

extern int nfsmount_main (int argc, char *argv[]);


extern int	optint;
extern int	opterr;
extern int	optopt;
extern char *	optarg;

static char *	progname = "trynfs";
static char *	version = VERSION;

static void
message (char *fmt, ...)
{
	va_list	ap;
	va_start (ap, fmt);
	fprintf (stderr, "%s: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, "\n");
	va_end (ap);
}

static void
fatal (char *fmt, ...)
{
	va_list	ap;
	va_start (ap, fmt);
	fprintf (stderr, "%s: ", progname);
	vfprintf (stderr, fmt, ap);
	fprintf (stderr, " (fatal)\n");
	va_end (ap);
	exit (1);
}

static void
usage (void)
{
	printf ("\
    %s [ -hv ] ip=... [ nfsroot=... ] mount-point \n\
    Activate network interfaces, then do NFS mount.\n\
    ip= and nfsroot= as in linux/Documentation/nfsroot.txt\n\
    If DHCP provides a root path, nfsroot server:path is overridden,\n\
    but NFS mount options still apply.\n\
    If no NFS options are given, mount is attempted first\n\
    without options, then with option 'udp', then with option 'v2'.\n\
\n\
    -h: usage\n\
    -v: version\n\
",
		progname);
}


int
do_ipconfig (int argc, char *argv[]) {
	int i, a = 0;
	char **args = alloca ((argc + 1) * sizeof (char *));
	if (! args) {
		/* behaviour undefined on stack overflow,
		 * so the following may be useless ...
		 */
		fatal ("out of memory");
	}

	args[a++] = "IP-Config";
	for (i = 1; i < argc; i++) {
		if (strncmp ("ip=", argv[i], 3) == 0
			|| strncmp ("nfsaddrs=", argv[i], 9) == 0)
		{
			args[a++] = argv[i];
		}
	}
	args[a] = NULL;
	return ipconfig_main (a, args);
}


int
do_nfsmount (char *nfsserver, char *nfspath, char *nfsopts, char *mountpoint)
{
	int	a = 0;
	char *	args[6];
	char *	fullpath = NULL;
		
	fullpath = alloca (strlen(nfsserver) + strlen(nfspath) + 7);
	if (!fullpath) {
		fatal ("out of memory");
	}
	sprintf (fullpath, "%s:%s", nfsserver, nfspath);

	args[a++] = "NFS-Mount";
	if (nfsopts) {
		args[a++] = "-o";
		args[a++] = nfsopts;
	}
	args[a++] = fullpath;
	args[a++] = mountpoint;
	args[a] = NULL;
	if (nfsopts) {
		message ("starting nfsmount -o %s %s %s",
				nfsopts, fullpath, mountpoint);
	}
	else {
		message ("starting nfsmount %s %s", fullpath, mountpoint);
	}
	return nfsmount_main (a, args);
}


int
main (int argc, char *argv[])
{
	int	i;
	char *	nfsroot = NULL;		/* from the nfsroot= argument */
	char *	mountpoint = NULL;	/* mount here on client */

	char *	nfsserver = NULL;	/* server as dotted decimal */
	char	buf[40];
	char *	nfspath = NULL;		/* path on server to mount */
	char *	nfsopts = NULL;		/* nfs options */
	int	rc;			/* exist status */

	while ((i = getopt (argc, argv, "hv")) >= 0) {
		switch (i) {
		case 'h':
			usage ();
			return 0;
		case 'v':
			message ("version %s", version);
			return 0;
		default:
			fatal ("unknown argument, -h for help");
		}
	}

	for (i = optind; i < argc; i++) {
		if (strncmp (argv[i], "ip=", 3) == 0) {
			continue;
		}
		else if (strncmp (argv[i], "nfsaddrs=", 9) == 0) {
			continue;
		}
		else if (strncmp (argv[i], "nfsroot=", 8) == 0) {
			if (nfsroot) {
				fatal ("duplicate nfsroot= argument");
			}
			nfsroot = argv[i]+8;
		}
		else {
			if (mountpoint) {
				fatal ("too many arguments");
			}
			mountpoint = argv[i];
		}
	}

	if (mountpoint == NULL) {
		fatal ("no mointpoint specified");
	}
	if (mountpoint[0] != '/') {
		fatal ("mointpoint is not an absolute path");
	}

	/*
	 * Get a working interface, plus any provided DHCP values.
	 */
	do_ipconfig (argc, argv);
	if (ifaces == NULL) {
		fatal ("no working network interfaces");
	}
	
	/*
	 * Good.  We have a working interface, now to
	 * mount it.
	 * First do options and choose between nfsroot= and dhcp reply
	 */


	if (nfsroot) {
		char *p = strchr (nfsroot, ',');
		if (p) {
			*p++ = '\0';
			if (p) {
				nfsopts = p;
			}
		}
	}

	if (strlen (ifaces->bootpath) > 0) {
		strcpy (buf, my_inet_ntoa(ifaces->ip_server));
		nfsserver = buf;
		nfspath = ifaces->bootpath;
	}
	else if (nfsroot) {
		char *p = strchr (nfsroot, ':');
		if (p) {
			*p++ = '\0';
			nfsserver = nfsroot;
			nfspath = p;
		}
		else {
			strcpy (buf, my_inet_ntoa(ifaces->ip_server));
			nfsserver = buf;
			nfspath = nfsroot;
		}
	}
	else {
		fatal ("no nfsroot on command line or DHCP reply");
	}

	/*
	 * Now try mounting.  If no options are given,
	 * we try some fallback options.
	 */
	if (nfsopts) {
		rc = do_nfsmount (nfsserver, nfspath, nfsopts, mountpoint);
	}
	else {
		rc = do_nfsmount (nfsserver, nfspath, NULL, mountpoint);
		if (rc != 0) {
			rc = do_nfsmount(nfsserver,nfspath,"udp",mountpoint);
		}
		if (rc != 0) {
			rc = do_nfsmount(nfsserver,nfspath,"v2",mountpoint);
		}
	}

	return rc;
}
