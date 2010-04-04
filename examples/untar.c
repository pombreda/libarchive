/*
 * This file is in the public domain.
 * Use it as you wish.
 */

/*
 * This is a compact tar extraction program using libtransform whose
 * primary goal is small executable size.  Statically linked, it can
 * be very small, depending in large part on how cleanly factored your
 * system libraries are.  Note that this uses the standard libtransform,
 * without any special recompilation.  The only functional concession
 * is that this program uses the uid/gid from the transform instead of
 * doing uname/gname lookups.  (Add a call to
 * transform_write_disk_set_standard_lookup() to enable uname/gname
 * lookups, but be aware that this can add 500k or more to a static
 * executable, depending on the system libraries, since user/group
 * lookups frequently pull in password, YP/LDAP, networking, and DNS
 * resolver libraries.)
 *
 * To build:
 * $ gcc -static -Wall -o untar untar.c -ltransform
 * $ strip untar
 *
 * NOTE: On some systems, you may need to add additional flags
 * to ensure that untar.c is compiled the same way as libtransform
 * was compiled.  In particular, Linux users will probably
 * have to add -D_FILE_OFFSET_BITS=64 to the command line above.
 *
 * For fun, statically compile the following simple hello.c program
 * using the same flags as for untar and compare the size:
 *
 * #include <stdio.h>
 * int main(int argc, char **argv) {
 *    printf("hello, world\n");
 *    return(0);
 * }
 *
 * You may be even more surprised by the compiled size of true.c listed here:
 *
 * int main(int argc, char **argv) {
 *    return (0);
 * }
 *
 * On a slightly customized FreeBSD 5 system that I used around
 * 2005, hello above compiled to 89k compared to untar of 69k.  So at
 * that time, libtransform's tar reader and extract-to-disk routines
 * compiled to less code than printf().
 *
 * On my FreeBSD development system today (August, 2009):
 *  hello: 195024 bytes
 *  true: 194912 bytes
 *  untar: 259924 bytes
 */

#include <sys/types.h>
__FBSDID("$FreeBSD$");

#include <sys/stat.h>

#include <transform.h>
#include <transform_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	errmsg(const char *);
static void	extract(const char *filename, int do_extract, int flags);
static void	fail(const char *, const char *, int);
static int	copy_data(struct transform *, struct transform *);
static void	msg(const char *);
static void	usage(void);
static void	warn(const char *, const char *);

static int verbose = 0;

int
main(int argc, const char **argv)
{
	const char *filename = NULL;
	int compress, flags, mode, opt;

	(void)argc;
	mode = 'x';
	verbose = 0;
	compress = '\0';
	flags = TRANSFORM_EXTRACT_TIME;

	/* Among other sins, getopt(3) pulls in printf(3). */
	while (*++argv != NULL && **argv == '-') {
		const char *p = *argv + 1;

		while ((opt = *p++) != '\0') {
			switch (opt) {
			case 'f':
				if (*p != '\0')
					filename = p;
				else
					filename = *++argv;
				p += strlen(p);
				break;
			case 'p':
				flags |= TRANSFORM_EXTRACT_PERM;
				flags |= TRANSFORM_EXTRACT_ACL;
				flags |= TRANSFORM_EXTRACT_FFLAGS;
				break;
			case 't':
				mode = opt;
				break;
			case 'v':
				verbose++;
				break;
			case 'x':
				mode = opt;
				break;
			default:
				usage();
			}
		}
	}

	switch (mode) {
	case 't':
		extract(filename, 0, flags);
		break;
	case 'x':
		extract(filename, 1, flags);
		break;
	}

	return (0);
}


static void
extract(const char *filename, int do_extract, int flags)
{
	struct transform *a;
	struct transform *ext;
	struct transform_entry *entry;
	int r;

	a = transform_read_new();
	ext = transform_write_disk_new();
	transform_write_disk_set_options(ext, flags);
	/*
	 * Note: transform_write_disk_set_standard_lookup() is useful
	 * here, but it requires library routines that can add 500k or
	 * more to a static executable.
	 */
	transform_read_support_format_tar(a);
	/*
	 * On my system, enabling other transform formats adds 20k-30k
	 * each.  Enabling gzip decompression adds about 20k.
	 * Enabling bzip2 is more expensive because the libbz2 library
	 * isn't very well factored.
	 */
	if (filename != NULL && strcmp(filename, "-") == 0)
		filename = NULL;
	if ((r = transform_read_open_file(a, filename, 10240)))
		fail("transform_read_open_file()",
		    transform_error_string(a), r);
	for (;;) {
		r = transform_read_next_header(a, &entry);
		if (r == TRANSFORM_EOF)
			break;
		if (r != TRANSFORM_OK)
			fail("transform_read_next_header()",
			    transform_error_string(a), 1);
		if (verbose && do_extract)
			msg("x ");
		if (verbose || !do_extract)
			msg(transform_entry_pathname(entry));
		if (do_extract) {
			r = transform_write_header(ext, entry);
			if (r != TRANSFORM_OK)
				warn("transform_write_header()",
				    transform_error_string(ext));
			else {
				copy_data(a, ext);
				r = transform_write_finish_entry(ext);
				if (r != TRANSFORM_OK)
					fail("transform_write_finish_entry()",
					    transform_error_string(ext), 1);
			}

		}
		if (verbose || !do_extract)
			msg("\n");
	}
	transform_read_close(a);
	transform_read_free(a);
	exit(0);
}

static int
copy_data(struct transform *ar, struct transform *aw)
{
	int r;
	const void *buff;
	size_t size;
	off_t offset;

	for (;;) {
		r = transform_read_data_block(ar, &buff, &size, &offset);
		if (r == TRANSFORM_EOF)
			return (TRANSFORM_OK);
		if (r != TRANSFORM_OK)
			return (r);
		r = transform_write_data_block(aw, buff, size, offset);
		if (r != TRANSFORM_OK) {
			warn("transform_write_data_block()",
			    transform_error_string(aw));
			return (r);
		}
	}
}

/*
 * These reporting functions use low-level I/O; on some systems, this
 * is a significant code reduction.  Of course, on many server and
 * desktop operating systems, malloc() and even crt rely on printf(),
 * which in turn pulls in most of the rest of stdio, so this is not an
 * optimization at all there.  (If you're going to pay 100k or more
 * for printf() anyway, you may as well use it!)
 */
static void
msg(const char *m)
{
	write(1, m, strlen(m));
}

static void
errmsg(const char *m)
{
	write(2, m, strlen(m));
}

static void
warn(const char *f, const char *m)
{
	errmsg(f);
	errmsg(" failed: ");
	errmsg(m);
	errmsg("\n");
}

static void
fail(const char *f, const char *m, int r)
{
	warn(f, m);
	exit(r);
}

static void
usage(void)
{
	const char *m = "Usage: untar [-tvx] [-f file] [file]\n";
	errmsg(m);
	exit(1);
}
