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

/*
 * Exercise the system-independent portion of the ACL support.
 * Check that archive_entry objects can save and restore NFS4 ACL data.
 *
 * This should work on all systems, regardless of whether local
 * filesystems support ACLs or not.
 */

struct acl_t {
	int type;  /* Type of entry: "allow" or "deny" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, everyone, etc. */
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

#if 0
static struct acl_t acls0[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, 0, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, 0, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_EVERYONE, 0, "" },
};
#endif

static struct acl_t acls1[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_READ_DATA,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DENY, ARCHIVE_ENTRY_ACL_WRITE_DATA,
	  ARCHIVE_ENTRY_ACL_EVERYONE, -1, "" },
};

#if 0
static struct acl_t acls2[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE | ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0,
	  ARCHIVE_ENTRY_ACL_USER, 78, "user78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_GROUP, 78, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
};
#endif

/*
 * POSIX.1e entry types; attempts to set these on top of NFS4
 * attributes should fail.
 */
#if 0
static struct acl_t acls_posix1e[] = {
	/* POSIX.1e ACL types */
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 78, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_DEFAULT, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER, 78, "" },

	/* POSIX.1e tags */
	{ ARCHIVE_ENTRY_ACL_TYPE_ALLOW, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
};
#endif

static void
set_acls(struct archive_entry *ae, struct acl_t *acls, int n)
{
	int i;

	archive_entry_acl_clear(ae);
	for (i = 0; i < n; i++) {
		assertEqualInt(ARCHIVE_OK,
		    archive_entry_acl_add_entry(ae,
			acls[i].type, acls[i].permset, acls[i].tag,
			acls[i].qual, acls[i].name));
	}
}

static int
acl_match(struct acl_t *acl, int type, int permset, int tag, int qual, const char *name)
{
	if (type != acl->type)
		return (0);
	if (permset != acl->permset)
		return (0);
	if (tag != acl->tag)
		return (0);
	if (tag == ARCHIVE_ENTRY_ACL_USER_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_EVERYONE)
		return (1);
	if (qual != acl->qual)
		return (0);
	if (name == NULL) {
		if (acl->name == NULL || acl->name[0] == '\0')
			return (1);
	}
	if (acl->name == NULL) {
		if (name[0] == '\0')
			return (1);
	}
	return (0 == strcmp(name, acl->name));
}

static void
compare_acls(struct archive_entry *ae, struct acl_t *acls, int n)
{
	int *marker = malloc(sizeof(marker[0]) * n);
	int i;
	int r;
	int type, permset, tag, qual;
	int matched;
	const char *name;

	for (i = 0; i < n; i++)
		marker[i] = i;

	while (0 == (r = archive_entry_acl_next(ae,
			 ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
			 &type, &permset, &tag, &qual, &name))) {
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(&acls[marker[i]], type, permset,
				tag, qual, name)) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}
		failure("Could not find match for ACL "
		    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
		    type, permset, tag, qual, name);
		assertEqualInt(1, matched);
	}
	assertEqualInt(ARCHIVE_EOF, r);
	failure("Could not find match for ACL "
	    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
	    acls[marker[0]].type, acls[marker[0]].permset,
	    acls[marker[0]].tag, acls[marker[0]].qual, acls[marker[0]].name);
	assertEqualInt(0, n); /* Number of ACLs not matched should == 0 */
	free(marker);
}

DEFINE_TEST(test_acl_nfs4)
{
	struct archive_entry *ae;

	/* Create a simple archive_entry. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
        archive_entry_set_mode(ae, S_IFREG | 0777);

	/* Store and read back some basic ACL entries. */
	set_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	assertEqualInt(4,
	    archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_NFS4));
	compare_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

#if 0
	/* A more extensive set of ACLs. */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertEqualInt(6, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]), 0543);
	failure("Basic ACLs should set mode to 0543, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0543);

	/*
	 * Check that clearing ACLs gets rid of them all by repeating
	 * the first test.
	 */
	set_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assert(0 == archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/*
	 * Different types of malformed ACL entries that should
	 * fail when added to existing POSIX.1e ACLs.
	 */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	for (i = 0; i < sizeof(acls_nfs4)/sizeof(acls_nfs4[0]); ++i) {
		struct acl_t *p = &acls_nfs4[i];
		failure("Malformed ACL test #%d", i);
		assertEqualInt(ARCHIVE_FAILED,
		    archive_entry_acl_add_entry(ae,
			p->type, p->permset, p->tag, p->qual, p->name));
		assertEqualInt(6,
		    archive_entry_acl_reset(ae,
			ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	}
#endif
	archive_entry_free(ae);
}
