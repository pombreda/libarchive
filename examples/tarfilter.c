/*
 * This file is in the public domain.
 *
 * Feel free to use it as you wish.
 */

/*
 * This example program reads an transform from stdin (which can be in
 * any format recognized by libtransform) and writes certain entries to
 * an uncompressed ustar transform on stdout.  This is a template for
 * many kinds of transform manipulation: converting formats, resetting
 * ownership, inserting entries, removing entries, etc.
 *
 * To compile:
 * gcc -Wall -o tarfilter tarfilter.c -ltransform -lz -lbz2
 */

#include <sys/stat.h>
#include <transform.h>
#include <transform_entry.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void
die(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	char buff[8192];
	ssize_t len;
	int r;
	mode_t m;
	struct transform *ina;
	struct transform *outa;
	struct transform_entry *entry;

	/* Read an transform from stdin, with automatic format detection. */
	ina = transform_read_new();
	if (ina == NULL)
		die("Couldn't create transform reader.");
	if (transform_read_support_compression_all(ina) != TRANSFORM_OK)
		die("Couldn't enable decompression");
	if (transform_read_support_format_all(ina) != TRANSFORM_OK)
		die("Couldn't enable read formats");
	if (transform_read_open_fd(ina, 0, 10240) != TRANSFORM_OK)
		die("Couldn't open input transform");

	/* Write an uncompressed ustar transform to stdout. */
	outa = transform_write_new();
	if (outa == NULL)
		die("Couldn't create transform writer.");
	if (transform_write_set_compression_none(outa) != TRANSFORM_OK)
		die("Couldn't enable compression");
	if (transform_write_set_format_ustar(outa) != TRANSFORM_OK)
		die("Couldn't set output format");
	if (transform_write_open_fd(outa, 1) != TRANSFORM_OK)
		die("Couldn't open output transform");

	/* Examine each entry in the input transform. */
	while ((r = transform_read_next_header(ina, &entry)) == TRANSFORM_OK) {
		fprintf(stderr, "%s: ", transform_entry_pathname(entry));

		/* Skip anything that isn't a regular file. */
		if (!S_ISREG(transform_entry_mode(entry))) {
			fprintf(stderr, "skipped\n");
			continue;
		}

		/* Make everything owned by root/wheel. */
		transform_entry_set_uid(entry, 0);
		transform_entry_set_uname(entry, "root");
		transform_entry_set_gid(entry, 0);
		transform_entry_set_gname(entry, "wheel");

		/* Make everything permission 0744, strip SUID, etc. */
		m = transform_entry_mode(entry);
		transform_entry_set_mode(entry, (m & ~07777) | 0744);

		/* Copy input entries to output transform. */
		if (transform_write_header(outa, entry) != TRANSFORM_OK)
			die("Error writing output transform");
		if (transform_entry_size(entry) > 0) {
			len = transform_read_data(ina, buff, sizeof(buff));
			while (len > 0) {
				if (transform_write_data(outa, buff, len) != len)
					die("Error writing output transform");
				len = transform_read_data(ina, buff, sizeof(buff));
			}
			if (len < 0)
				die("Error reading input transform");
		}
		fprintf(stderr, "copied\n");
	}
	if (r != TRANSFORM_EOF)
		die("Error reading transform");
	/* Close the transforms.  */
	if (transform_read_free(ina) != TRANSFORM_OK)
		die("Error closing input transform");
	if (transform_write_free(outa) != TRANSFORM_OK)
		die("Error closing output transform");
	return (0);
}
