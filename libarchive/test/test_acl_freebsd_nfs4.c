/*-
 * Copyright (c) 2003-2010 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

#if defined(__FreeBSD__) && __FreeBSD__ >= 8
#define _ACL_PRIVATE
#include <sys/acl.h>

struct myacl_t {
	int type;
	int permset;
	int tag;
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct myacl_t acls_reg[] = {
	/* For this test, we need to be able to read and write the ACL. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
	  ARCHIVE_ENTRY_ACL_READ_ACL | ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, ""},

	/* An entry for each type. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 108, "user108" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 109, "user109" },
#if 0
	/* An entry for each permission. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 112, "user112" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 113, "user113" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_LIST_DIRECTORY,
//	  ARCHIVE_ENTRY_ACL_USER, 114, "user114" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 115, "user115" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_FILE,
//	  ARCHIVE_ENTRY_ACL_USER, 116, "user116" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_APPEND_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 117, "user117" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_ADD_SUBDIRECTORY,
//	  ARCHIVE_ENTRY_ACL_USER, 118, "user118" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 119, "user119" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_NAMED_ATTRS,
	  ARCHIVE_ENTRY_ACL_USER, 120, "user120" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE_CHILD,
//	  ARCHIVE_ENTRY_ACL_USER, 121, "user121" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 122, "user122" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ATTRIBUTES,
	  ARCHIVE_ENTRY_ACL_USER, 123, "user123" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_DELETE,
	  ARCHIVE_ENTRY_ACL_USER, 124, "user124" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 125, "user125" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_ACL,
	  ARCHIVE_ENTRY_ACL_USER, 126, "user126" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_OWNER,
	  ARCHIVE_ENTRY_ACL_USER, 127, "user127" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_SYNCHRONIZE,
//	  ARCHIVE_ENTRY_ACL_USER, 128, "user128" },

	/* One entry with each inheritance value. */
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FILE_INHERIT,
//	  ARCHIVE_ENTRY_ACL_USER, 129, "user129" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_DIRECTORY_INHERIT,
//	  ARCHIVE_ENTRY_ACL_USER, 130, "user130" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_NO_PROPAGATE_INHERIT,
//	  ARCHIVE_ENTRY_ACL_USER, 131, "user131" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_INHERIT_ONLY,
//	  ARCHIVE_ENTRY_ACL_USER, 132, "user132" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_SUCCESSFUL_ACCESS,
//	  ARCHIVE_ENTRY_ACL_USER, 133, "user133" },
//	{ ARCHIVE_ENTRY_ACL_TYPE_AUDIT,
//	  ARCHIVE_ENTRY_ACL_READ_DATA | ARCHIVE_ENTRY_ACL_ENTRY_FAILED_ACCESS,
//	  ARCHIVE_ENTRY_ACL_USER, 134, "user134" },

	/* One entry for each qualifier. */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 135, "user135" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP, 136, "group136" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
#endif
	{ 0, 0, 0, 0, NULL }
};

static void
set_acls(struct archive_entry *ae, struct myacl_t *acls)
{
	int i;

	archive_entry_acl_clear(ae);
	for (i = 0; acls[i].name != NULL; i++) {
		assertEqualInt(ARCHIVE_OK,
		    archive_entry_acl_add_entry(ae,
			acls[i].type, acls[i].permset, acls[i].tag,
			acls[i].qual, acls[i].name));
	}
}

static int
acl_match(acl_entry_t aclent, struct myacl_t *myacl)
{
	gid_t g, *gp;
	uid_t u, *up;
	acl_tag_t tag_type;
	acl_permset_t opaque_ps;
	int permset = 0;

	acl_get_tag_type(aclent, &tag_type);

	/* translate the silly opaque permset to a bitmap */
	acl_get_permset(aclent, &opaque_ps);
	if (acl_get_perm_np(opaque_ps, ACL_EXECUTE))
		permset |= ARCHIVE_ENTRY_ACL_EXECUTE;
	if (acl_get_perm_np(opaque_ps, ACL_WRITE))
		permset |= ARCHIVE_ENTRY_ACL_WRITE;
	if (acl_get_perm_np(opaque_ps, ACL_READ))
		permset |= ARCHIVE_ENTRY_ACL_READ;
	if (acl_get_perm_np(opaque_ps, ACL_READ_DATA))
		permset |= ARCHIVE_ENTRY_ACL_READ_DATA;
	if (acl_get_perm_np(opaque_ps, ACL_READ_ACL))
		permset |= ARCHIVE_ENTRY_ACL_READ_ACL;
	if (acl_get_perm_np(opaque_ps, ACL_WRITE_ACL))
		permset |= ARCHIVE_ENTRY_ACL_WRITE_ACL;

	if (permset != myacl->permset)
		return (0);

	switch (tag_type) {
	case ACL_USER_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER_OBJ) return (0);
		break;
	case ACL_USER:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_USER)
			return (0);
		up = acl_get_qualifier(aclent);
		u = *up;
		acl_free(up);
		if ((uid_t)myacl->qual != u)
			return (0);
		break;
	case ACL_GROUP_OBJ:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP_OBJ) return (0);
		break;
	case ACL_GROUP:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_GROUP)
			return (0);
		gp = acl_get_qualifier(aclent);
		g = *gp;
		acl_free(gp);
		if ((gid_t)myacl->qual != g)
			return (0);
		break;
	case ACL_MASK:
		if (myacl->tag != ARCHIVE_ENTRY_ACL_MASK) return (0);
		break;
	}
	return (1);
}

static void
compare_acls(acl_t acl, struct myacl_t *myacls)
{
	int *marker;
	int entry_id = ACL_FIRST_ENTRY;
	int matched;
	int i, n;
	acl_entry_t acl_entry;

	/* Count ACL entries in myacls array and allocate an indirect array. */
	for (n = 0; myacls[n].name != NULL; ++n)
		continue;
	marker = malloc(sizeof(marker[0]) * n);
	for (i = 0; i < n; i++)
		marker[i] = i;

	/*
	 * Iterate over acls in system acl object, try to match each
	 * one with an item in the myacls array.
	 */
	while (1 == acl_get_entry(acl, entry_id, &acl_entry)) {
		/* After the first time... */
		entry_id = ACL_NEXT_ENTRY;

		/* Search for a matching entry (tag and qualifier) */
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(acl_entry, &myacls[marker[i]])) {
fprintf(stderr, "Found match!\n");
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}

		/* TODO: Print out more details in this case. */
		failure("ACL entry on file that shouldn't be there");
		assert(matched == 1);
	}

	/* Dump entries in the myacls array that weren't in the system acl. */
	for (i = 0; i < n; ++i) {
		failure(" ACL entry %d missing from file: "
		    "type=%d,permset=%d,tag=%d,qual=%d,name=``%s''\n",
		    i, myacls[marker[i]].type, myacls[marker[i]].permset,
		    myacls[marker[i]].tag, myacls[marker[i]].qual,
		    myacls[marker[i]].name);
		assert(0); /* Record this as a failure. */
	}
	free(marker);
}
#endif

/*
 * Verify ACL restore-to-disk.  This test is FreeBSD-specific.
 */

DEFINE_TEST(test_acl_freebsd_nfs4)
{
#if !defined(__FreeBSD__)
	skipping("FreeBSD-specific NFS4 ACL restore test");
#elif __FreeBSD__ < 8
	skipping("NFS4 ACLs supported only on FreeBSD 8.0 and later");
#else
	struct stat st;
	struct archive *a;
	struct archive_entry *ae;
	int n, fd;
	acl_t acl;

	/*
	 * First, do a quick manual set/read of ACL data to
	 * verify that the local filesystem does support ACLs.
	 * If it doesn't, we'll simply skip the remaining tests.
	 */
	acl = acl_from_text("owner@:rwxp::allow,group@:rwp:f:allow");
	assert((void *)acl != NULL);
	/* Create a test dir and try to set an ACL on it. */
	if (!assertMakeDir("pretest", 0755)) {
		acl_free(acl);
		return;
	}

	n = acl_set_file("pretest", ACL_TYPE_NFS4, acl);
	acl_free(acl);
	if (n != 0 && errno == EOPNOTSUPP) {
		close(fd);
		skipping("NFS4 ACL tests require that NFS4 ACLs"
		    " be enabled on the filesystem");
		return;
	}
	if (n != 0 && errno == EINVAL) {
		close(fd);
		skipping("This filesystem does not support NFS4 ACLs");
		return;
	}
	failure("acl_set_fd(): errno = %d (%s)",
	    errno, strerror(errno));
	assertEqualInt(0, n);
	close(fd);

	/* Create a write-to-disk object. */
	assert(NULL != (a = archive_write_disk_new()));
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL);

	/* Populate an archive entry with some metadata, including ACL info */
	ae = archive_entry_new();
	assert(ae != NULL);
	archive_entry_set_pathname(ae, "test0");
	archive_entry_set_filetype(ae, AE_IFREG);
	archive_entry_set_perm(ae, 0654);
	archive_entry_set_mtime(ae, 123456, 7890);
	archive_entry_set_size(ae, 0);
	set_acls(ae, acls_reg);

	/* Write the entry to disk, including ACLs. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Verify the data on disk. */
	assertEqualInt(0, stat("test0", &st));
	assertEqualInt(st.st_mtime, 123456);
	acl = acl_get_file("test0", ACL_TYPE_NFS4);
	assert(acl != (acl_t)NULL);
	compare_acls(acl, acls_reg);
	acl_free(acl);
#endif
}
