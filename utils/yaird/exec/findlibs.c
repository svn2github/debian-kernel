/* findlibs.c: find libraries used by an ELF executable.

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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <elf.h>

#include <config.h>

extern int	optint;
extern int	opterr;
extern int	optopt;
extern char *	optarg;

static int	silent = 0;		/* dont do error messages */
static char *	progname = "findlibs";
static char *	version = VERSION;

void
message (char *fmt, ...)
{
	va_list	ap;
	if (! silent) {
		va_start (ap, fmt);
		fprintf (stderr, "%s: ", progname);
		vfprintf (stderr, fmt, ap);
		fprintf (stderr, "\n");
		va_end (ap);
	}
}

void
fatal (char *fmt, ...)
{
	va_list	ap;
	if (! silent) {
		va_start (ap, fmt);
		fprintf (stderr, "%s: ", progname);
		vfprintf (stderr, fmt, ap);
		fprintf (stderr, " (fatal)\n");
		va_end (ap);
	}
	exit (1);
}

void
usage (void)
{
	printf ("\
    %s [ -qhv ] executable\n\
    print dynamic loader and basenames of shared libraries for executable\n\
    -q: do not print error messages, just give exit code\n\
    -h: this help text\n\
    -v: version\n\
    corrupt ELF files cause an error exit status, non ELF-files\n\
    or non-executable ELF files do not.\n\
",
		progname);
}

void *
getStuff (FILE *fp, Elf32_Off offset, Elf32_Word size)
{
	int		rc;
	size_t		rc2;
	void *		ptr = NULL;

	ptr = malloc(size);
	if (! ptr) {
		fatal ("could not allocate memory");
	}

	rc = fseek (fp, offset, SEEK_SET);
	if (rc != 0) {
		fatal ("could not seek");
	}
	rc2 = fread (ptr, size, 1, fp);
	if (rc2 != 1) {
		fatal ("could not read");
	}
	return ptr;
}

void
doOne32 (char *filename, FILE *fp)
{
	int		i;
	Elf32_Ehdr *	ehdr = NULL;
	Elf32_Phdr *	phdr = NULL;
	int		interpIndex = -1;	/* prog hdr with interp. */
	Elf32_Shdr *	shdrs = NULL;
	int		dynamicIndex = -1;	/* section named .dynamic */
	Elf32_Dyn *	dynamic = NULL;
	int		dynstrIndex = -1;	/* section named .dynstr */
	char *		dynstr = NULL;
	char *		sectionNames = NULL;


	ehdr = getStuff (fp, 0, sizeof(*ehdr));

	if (ehdr->e_ehsize != sizeof(*ehdr)) {
		fatal ("bad size ehdr %s", filename);
	}

	if (! (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)) {
		message ("bad type ehdr %s", filename);
		goto out;
	}

	if (ehdr->e_phentsize != sizeof (*phdr)) {
		fatal ("bad size phdr %s", filename);
	}

	if (ehdr->e_phnum < 1 || ehdr->e_phnum > (65336/sizeof(*phdr))) {
		fatal ("bad count phdr %s", filename);
	}

	phdr = getStuff (fp, ehdr->e_phoff, ehdr->e_phnum * sizeof(*phdr));

	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_INTERP) {
			if (interpIndex >= 0) {
				fatal ("corrupt image, interp %s", filename);
			}
			interpIndex = i;
		}
	}

	/*
	 * Print interpreter.
	 * Note you can have files with DT_NEEDED without interp;
	 * shared libraries are an example.
	 */
	if (interpIndex >= 0) {
		Elf32_Off	offset = phdr[interpIndex].p_offset;
		Elf32_Word	size = phdr[interpIndex].p_filesz;
		char *		interpreter = NULL;

		if (size < 2 || size > PATH_MAX) {
			fatal ("funny interpreter %s", filename);
		}

		interpreter = getStuff (fp, offset, size);
		if (interpreter[size - 1] != '\0') {
			fatal ("broken interpreter %s", filename);
		}

		printf ("interpreter: %s\n", interpreter);

		free (interpreter);
	}

	/*
	 * read section headers
	 */
	if (ehdr->e_shentsize != sizeof (Elf32_Shdr)) {
		fatal ("bad section header size %s", filename);
	}
	shdrs = getStuff (fp, ehdr->e_shoff, sizeof(*shdrs) * ehdr->e_shnum);

	/*
	 * read section header names
	 */
	if (ehdr->e_shstrndx >= ehdr->e_shnum) {
		fatal ("no section names in %s", filename);
	}
	sectionNames = getStuff (fp, shdrs[ehdr->e_shstrndx].sh_offset,
			shdrs[ehdr->e_shstrndx].sh_size);


	/*
	 * Find .dynamic section and corresponding strings.
	 */
	for (i = 0; i < ehdr->e_shnum; i++) {
		char *	name = &sectionNames[shdrs[i].sh_name];
		if (strcmp (name, ".dynamic") == 0) {
			dynamicIndex = i;
		}
		else if (strcmp (name, ".dynstr") == 0) {
			dynstrIndex = i;
		}
	}

	if (dynamicIndex < 0) {
		goto out;
	}

	if (dynamicIndex >= 0  &&  dynstrIndex < 0) {
		fatal ("corrupt dynamic without dynstr %s", filename);
	}
	if (dynamicIndex > ehdr->e_shnum) {
		fatal ("dynamic out of range %s", filename);
	}
	if (dynstrIndex > ehdr->e_shnum) {
		fatal ("dynamic out of range %s", filename);
	}
	dynamic = getStuff (fp, shdrs[dynamicIndex].sh_offset,
			shdrs[dynamicIndex].sh_size);
	dynstr = getStuff (fp, shdrs[dynstrIndex].sh_offset,
			shdrs[dynstrIndex].sh_size);

	for (i = 0;; i++) {
		if (dynamic[i].d_tag == DT_NEEDED) {
			Elf32_Word offset = dynamic[i].d_un.d_val;
			char *name = &dynstr[offset];

			printf ("needed: %s\n", name);
		}
		if (dynamic[i].d_tag == DT_NULL) {
			break;
		}
	}

out:
	if (dynstr) free (dynstr);
	if (dynamic) free (dynamic);
	if (ehdr) free (ehdr);
	if (phdr) free (phdr);
	if (shdrs) free (shdrs);
	if (sectionNames) free (sectionNames);
}


void
doOne64 (char *filename, FILE *fp)
{
	int		i;
	Elf64_Ehdr *	ehdr = NULL;
	Elf64_Phdr *	phdr = NULL;
	int		interpIndex = -1;	/* proghdr has interp. */
	Elf64_Shdr *	shdrs = NULL;
	int		dynamicIndex = -1;	/* section named .dynamic */
	Elf64_Dyn *	dynamic = NULL;
	int		dynstrIndex = -1;	/* section named .dynstr */
	char *		dynstr = NULL;
	char *		sectionNames = NULL;


	ehdr = getStuff (fp, 0, sizeof(*ehdr));

	if (ehdr->e_ehsize != sizeof(*ehdr)) {
		fatal ("bad size ehdr %s", filename);
	}

	if (! (ehdr->e_type == ET_EXEC || ehdr->e_type == ET_DYN)) {
		message ("bad type ehdr %s", filename);
		goto out;
	}

	if (ehdr->e_phentsize != sizeof (*phdr)) {
		fatal ("bad size phdr %s", filename);
	}

	if (ehdr->e_phnum < 1 || ehdr->e_phnum > (65336/sizeof(*phdr))) {
		fatal ("bad count phdr %s", filename);
	}

	phdr = getStuff (fp, ehdr->e_phoff, ehdr->e_phnum * sizeof(*phdr));

	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr[i].p_type == PT_INTERP) {
			if (interpIndex >= 0) {
				fatal ("corrupt image, interp %s", filename);
			}
			interpIndex = i;
		}
	}

	/*
	 * Print interpreter.
	 * Note you can have files with DT_NEEDED without interp;
	 * shared libraries are an example.
	 */
	if (interpIndex >= 0) {
		Elf64_Off	offset = phdr[interpIndex].p_offset;
		Elf64_Xword	size = phdr[interpIndex].p_filesz;
		char *		interpreter = NULL;

		if (size < 2 || size > PATH_MAX) {
			fatal ("funny interpreter %s", filename);
		}

		interpreter = getStuff (fp, offset, size);
		if (interpreter[size - 1] != '\0') {
			fatal ("broken interpreter %s", filename);
		}

		printf ("interpreter: %s\n", interpreter);

		free (interpreter);
	}

	/*
	 * read section headers
	 */
	if (ehdr->e_shentsize != sizeof (Elf64_Shdr)) {
		fatal ("bad section header size %s", filename);
	}
	shdrs = getStuff (fp, ehdr->e_shoff, sizeof(*shdrs) * ehdr->e_shnum);

	/*
	 * read section header names
	 */
	if (ehdr->e_shstrndx >= ehdr->e_shnum) {
		fatal ("no section names in %s", filename);
	}
	sectionNames = getStuff (fp, shdrs[ehdr->e_shstrndx].sh_offset,
			shdrs[ehdr->e_shstrndx].sh_size);

	for (i = 0; i < ehdr->e_shnum; i++) {
		char *	name = &sectionNames[shdrs[i].sh_name];
		if (strcmp (name, ".dynamic") == 0) {
			dynamicIndex = i;
		}
		else if (strcmp (name, ".dynstr") == 0) {
			dynstrIndex = i;
		}
	}

	if (dynamicIndex < 0) {
		goto out;
	}

	if (dynamicIndex >= 0  &&  dynstrIndex < 0) {
		fatal ("corrupt dynamic without dynstr %s", filename);
	}
	if (dynamicIndex > ehdr->e_shnum) {
		fatal ("dynamic out of range %s", filename);
	}
	if (dynstrIndex > ehdr->e_shnum) {
		fatal ("dynamic out of range %s", filename);
	}
	dynamic = getStuff (fp, shdrs[dynamicIndex].sh_offset,
			shdrs[dynamicIndex].sh_size);
	dynstr = getStuff (fp, shdrs[dynstrIndex].sh_offset,
			shdrs[dynstrIndex].sh_size);

	for (i = 0;; i++) {
		if (dynamic[i].d_tag == DT_NEEDED) {
			Elf64_Xword offset = dynamic[i].d_un.d_val;
			char *name = &dynstr[offset];

			printf ("needed: %s\n", name);
		}
		if (dynamic[i].d_tag == DT_NULL) {
			break;
		}
	}

out:
	if (dynstr) free (dynstr);
	if (dynamic) free (dynamic);
	if (ehdr) free (ehdr);
	if (phdr) free (phdr);
	if (shdrs) free (shdrs);
	if (sectionNames) free (sectionNames);
}


void
doOne (char *filename)
{
	FILE *		fp;
	size_t		rc;
	unsigned char	magic[EI_NIDENT];

	fp = fopen (filename, "r");
	if (! fp) {
		fatal ("could not open %s", filename);
	}

	rc = fread (magic, 1, EI_NIDENT, fp);
	if (rc < EI_NIDENT) {
		message ("not an ELF file: %s", filename);
		return;
	}

	if (strncmp (magic, ELFMAG, SELFMAG) != 0) {
		message ("not an ELF file: bad magic in %s", filename);
		return;
	}

	switch (magic[EI_CLASS]) {
	case ELFCLASS32:
		doOne32 (filename, fp);
		break;
	case ELFCLASS64:
		doOne64 (filename, fp);
		break;
	default:
		fatal ("bad class %s", filename);
	}

	if (fclose (fp)) {
		fatal ("could not close %s", filename);
	}
}

int
main (int argc, char *argv[])
{
	int	i;
	while ((i = getopt (argc, argv, "hvq")) >= 0) {
		switch (i) {
		case 'h':
			usage ();
			return 0;
		case 'v':
			message ("version %s", version);
			return 0;
		case 'q':
			silent = 1;
			break;
		default:
			fatal ("bad argument %c, -h for help", optopt);
		}
	}
	if (optind != argc -1) {
		fatal ("too many arguments");
	}

	doOne (argv[optind]);
	return 0;
}
