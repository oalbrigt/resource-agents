/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>
#include <asm/uaccess.h>
#include <linux/xattr.h>
#include <linux/xattr_acl.h>

#include "gfs.h"
#include "acl.h"
#include "eaops.h"
#include "eattr.h"

/**
 * gfs_ea_name2type - get the type of the ea, and trucate the type from the name
 * @namep: ea name, possibly with type appended
 *
 * Returns: GFS_EATYPE_XXX
 */

unsigned int
gfs_ea_name2type(const char *name, char **truncated_name)
{
	unsigned int type;

	if (strncmp(name, "system.", 7) == 0) {
		type = GFS_EATYPE_SYS;
		if (truncated_name)
			*truncated_name = strchr(name, '.') + 1;
	} else if (strncmp(name, "user.", 5) == 0) {
		type = GFS_EATYPE_USR;
		if (truncated_name)
			*truncated_name = strchr(name, '.') + 1;
	} else {
		type = GFS_EATYPE_UNUSED;
		if (truncated_name)
			*truncated_name = NULL;
	}

	return type;
}

/**
 * user_eo_get -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
user_eo_get(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	{
		struct inode *inode = ip->i_vnode;
		int error = permission(inode, MAY_READ, NULL);
		if (error)
			return error;
	}

	return gfs_ea_get_i(ip, er);
}

/**
 * user_eo_set -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
user_eo_set(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	{
		struct inode *inode = ip->i_vnode;
		if (S_ISREG(inode->i_mode) ||
		    (S_ISDIR(inode->i_mode) && !(inode->i_mode & S_ISVTX))) {
			int error = permission(inode, MAY_WRITE, NULL);
			if (error)
				return error;
		} else
			return -EPERM;
	}

	return gfs_ea_set_i(ip, er);
}

/**
 * user_eo_remove -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
user_eo_remove(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	{
		struct inode *inode = ip->i_vnode;
		if (S_ISREG(inode->i_mode) ||
		    (S_ISDIR(inode->i_mode) && !(inode->i_mode & S_ISVTX))) {
			int error = permission(inode, MAY_WRITE, NULL);
			if (error)
				return error;
		} else
			return -EPERM;
	}

	return gfs_ea_remove_i(ip, er);
}

/**
 * system_eo_get -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
system_eo_get(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	if (!GFS_ACL_IS_ACCESS(er->er_name, er->er_name_len) &&
	    !GFS_ACL_IS_DEFAULT(er->er_name, er->er_name_len) &&
	    !capable(CAP_SYS_ADMIN))
		return -EPERM;

	return gfs_ea_get_i(ip, er);	
}

/**
 * system_eo_set -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
system_eo_set(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	if (GFS_ACL_IS_ACCESS(er->er_name, er->er_name_len)) {
		int remove = FALSE;
		int error;

		er->er_mode = ip->i_vnode->i_mode;
		error = gfs_acl_validate_set(ip, TRUE, er,
					     &er->er_mode, &remove);
		if (error)
			return error;
		error = gfs_ea_set_i(ip, er);
		if (error)
			return error;
		if (remove)
			gfs_ea_remove_i(ip, er);
		return 0;

	} else if (GFS_ACL_IS_DEFAULT(er->er_name, er->er_name_len)) {
		int error = gfs_acl_validate_set(ip, FALSE, er,
						 NULL, NULL);
		if (error)
			return error;
		return gfs_ea_set_i(ip, er);

	}

	return -EPERM;
}

/**
 * system_eo_remove -
 * @ip:
 * @er:
 *
 * Returns: errno
 */

static int
system_eo_remove(struct gfs_inode *ip, struct gfs_ea_request *er)
{
	if (GFS_ACL_IS_ACCESS(er->er_name, er->er_name_len)) {
		int error = gfs_acl_validate_remove(ip, TRUE);
		if (error)
			return error;

	} else if (GFS_ACL_IS_DEFAULT(er->er_name, er->er_name_len)) {
		int error = gfs_acl_validate_remove(ip, FALSE);
		if (error)
			return error;

	} else
	        return -EPERM;

	return gfs_ea_remove_i(ip, er);	
}

struct gfs_eattr_operations gfs_user_eaops = {
	.eo_get = user_eo_get,
	.eo_set = user_eo_set,
	.eo_remove = user_eo_remove,
	.eo_name = "user",
};

struct gfs_eattr_operations gfs_system_eaops = {
	.eo_get = system_eo_get,
	.eo_set = system_eo_set,
	.eo_remove = system_eo_remove,
	.eo_name = "system",
};

struct gfs_eattr_operations *gfs_ea_ops[] = {
	NULL,
	&gfs_user_eaops,
	&gfs_system_eaops,
};


