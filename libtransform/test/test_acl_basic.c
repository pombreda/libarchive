/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
__FBSDID("$FreeBSD: src/lib/libtransform/test/test_acl_basic.c,v 1.6 2008/10/19 00:13:57 kientzle Exp $");

/*
 * Exercise the system-independent portion of the ACL support.
 * Check that transform_entry objects can save and restore ACL data.
 *
 * This should work on all systems, regardless of whether local
 * filesystems support ACLs or not.
 */

struct acl_t {
	int type;  /* Type of ACL: "access" or "default" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, other, etc. */
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct acl_t acls0[] = {
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_EXECUTE,
	  TRANSFORM_ENTRY_ACL_USER_OBJ, 0, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_GROUP_OBJ, 0, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_WRITE,
	  TRANSFORM_ENTRY_ACL_OTHER, 0, "" },
};

static struct acl_t acls1[] = {
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_EXECUTE,
	  TRANSFORM_ENTRY_ACL_USER_OBJ, -1, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_USER, 77, "user77" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_WRITE,
	  TRANSFORM_ENTRY_ACL_OTHER, -1, "" },
};

static struct acl_t acls2[] = {
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_EXECUTE | TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_USER_OBJ, -1, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_USER, 77, "user77" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, 0,
	  TRANSFORM_ENTRY_ACL_USER, 78, "user78" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_READ,
	  TRANSFORM_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, 0007,
	  TRANSFORM_ENTRY_ACL_GROUP, 78, "group78" },
	{ TRANSFORM_ENTRY_ACL_TYPE_ACCESS, TRANSFORM_ENTRY_ACL_WRITE | TRANSFORM_ENTRY_ACL_EXECUTE,
	  TRANSFORM_ENTRY_ACL_OTHER, -1, "" },
};

static void
set_acls(struct transform_entry *ae, struct acl_t *acls, int n)
{
	int i;

	transform_entry_acl_clear(ae);
	for (i = 0; i < n; i++) {
		transform_entry_acl_add_entry(ae,
		    acls[i].type, acls[i].permset, acls[i].tag, acls[i].qual,
		    acls[i].name);
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
	if (tag == TRANSFORM_ENTRY_ACL_USER_OBJ)
		return (1);
	if (tag == TRANSFORM_ENTRY_ACL_GROUP_OBJ)
		return (1);
	if (tag == TRANSFORM_ENTRY_ACL_OTHER)
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
compare_acls(struct transform_entry *ae, struct acl_t *acls, int n, int mode)
{
	int *marker = malloc(sizeof(marker[0]) * n);
	int i;
	int r;
	int type, permset, tag, qual;
	int matched;
	const char *name;

	for (i = 0; i < n; i++)
		marker[i] = i;

	while (0 == (r = transform_entry_acl_next(ae,
			 TRANSFORM_ENTRY_ACL_TYPE_ACCESS,
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
		if (tag == TRANSFORM_ENTRY_ACL_USER_OBJ) {
			if (!matched) printf("No match for user_obj perm\n");
			failure("USER_OBJ permset (%02o) != user mode (%02o)",
			    permset, 07 & (mode >> 6));
			assert((permset << 6) == (mode & 0700));
		} else if (tag == TRANSFORM_ENTRY_ACL_GROUP_OBJ) {
			if (!matched) printf("No match for group_obj perm\n");
			failure("GROUP_OBJ permset %02o != group mode %02o",
			    permset, 07 & (mode >> 3));
			assert((permset << 3) == (mode & 0070));
		} else if (tag == TRANSFORM_ENTRY_ACL_OTHER) {
			if (!matched) printf("No match for other perm\n");
			failure("OTHER permset (%02o) != other mode (%02o)",
			    permset, mode & 07);
			assert((permset << 0) == (mode & 0007));
		} else {
			failure("Could not find match for ACL "
			    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
			    type, permset, tag, qual, name);
			assert(matched == 1);
		}
	}
	assertEqualInt(TRANSFORM_EOF, r);
	assert((mode & 0777) == (transform_entry_mode(ae) & 0777));
	failure("Could not find match for ACL "
	    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
	    acls[marker[0]].type, acls[marker[0]].permset,
	    acls[marker[0]].tag, acls[marker[0]].qual, acls[marker[0]].name);
	assert(n == 0); /* Number of ACLs not matched should == 0 */
	free(marker);
}

DEFINE_TEST(test_acl_basic)
{
	struct transform_entry *ae;

	/* Create a simple transform_entry. */
	assert((ae = transform_entry_new()) != NULL);
	transform_entry_set_pathname(ae, "file");
        transform_entry_set_mode(ae, S_IFREG | 0777);

	/* Basic owner/owning group should just update mode bits. */
	set_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assert(0 == transform_entry_acl_reset(ae, TRANSFORM_ENTRY_ACL_TYPE_ACCESS));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    transform_entry_mode(ae)&0777);
	assert((transform_entry_mode(ae) & 0777) == 0142);


	/* With any extended ACL entry, we should read back a full set. */
	set_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	failure("One extended ACL should flag all ACLs to be returned.");
	assert(4 == transform_entry_acl_reset(ae, TRANSFORM_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]), 0142);
	failure("Basic ACLs should set mode to 0142, not %04o",
	    transform_entry_mode(ae)&0777);
	assert((transform_entry_mode(ae) & 0777) == 0142);


	/* A more extensive set of ACLs. */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertEqualInt(6, transform_entry_acl_reset(ae, TRANSFORM_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]), 0543);
	failure("Basic ACLs should set mode to 0543, not %04o",
	    transform_entry_mode(ae)&0777);
	assert((transform_entry_mode(ae) & 0777) == 0543);

	/*
	 * Check that clearing ACLs gets rid of them all by repeating
	 * the first test.
	 */
	set_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assert(0 == transform_entry_acl_reset(ae, TRANSFORM_ENTRY_ACL_TYPE_ACCESS));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    transform_entry_mode(ae)&0777);
	assert((transform_entry_mode(ae) & 0777) == 0142);
	transform_entry_free(ae);
}
