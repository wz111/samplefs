/*
 *   fs/samplefs/super.c
 *
 *   Copyright (C) International Business Machines  Corp., 2006, 2007
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Sample File System
 *
 *   Primitive example to show how to create a Linux filesystem module
 *
 *   superblock related and misc. functions
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#include <linux/nls.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/seq_file.h>
#include "samplefs.h"

/* helpful if this is different than other fs */
#define SAMPLEFS_MAGIC     0x73616d70 /* "SAMP" */

unsigned int sample_parm = 0;
module_param(sample_parm, int, 0);
MODULE_PARM_DESC(sample_parm, "An example parm. Default: x Range: y to z");

static void
samplefs_put_super(struct super_block *sb)
{
	struct samplefs_sb_info *sfs_sb;

	sfs_sb = SFS_SB(sb);
	if (sfs_sb == NULL) {
		/* Empty superblock info passed to unmount */
		return;
	}

	unload_nls(sfs_sb->local_nls);
 
	/* FS-FILLIN your fs specific umount logic here */

	kfree(sfs_sb);
	return;
}


struct super_operations samplefs_super_ops = {
	.statfs         = simple_statfs,
	.drop_inode     = generic_delete_inode, /* Not needed, is the default */
	.put_super      = samplefs_put_super,
};

static void
samplefs_parse_mount_options(char *options, struct samplefs_sb_info *sfs_sb)
{
	char *value;
	char *data;
	int size;

	if (!options)
		return;

	printk(KERN_INFO "samplefs: parsing mount options %s\n", options);

	while ((data = strsep(&options, ",")) != NULL) {
		if (!*data)
			continue;
		if ((value = strchr(data, '=')) != NULL)
			*value++ = '\0';

		if (strncasecmp(data, "rsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0) {
					sfs_sb->rsize = size;
					printk(KERN_INFO
						"samplefs: rsize %d\n", size);
				}

			}
		} else if (strncasecmp(data, "wsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0) {
					sfs_sb->wsize = size;
					printk(KERN_INFO
						"samplefs: wsize %d\n", size);
				}
			}
		} else if ((strncasecmp(data, "nocase", 6) == 0) ||
			   (strncasecmp(data, "ignorecase", 10)  == 0)) {
			sfs_sb->flags |= SFS_MNT_CASE;
			printk(KERN_INFO "samplefs: ignore case\n");

		} else {
			printk(KERN_WARNING "samplefs: bad mount option %s\n",
				data);
		} 
	}
}

static int sfs_ci_hash(const struct dentry *dentry, struct qstr *q)
{
        struct nls_table *codepage = SFS_SB(dentry->d_inode->i_sb)->local_nls;
        unsigned long hash;
        int i;

//        hash = init_name_hash(hash);
        for (i = 0; i < q->len; i++)
                hash = partial_name_hash(nls_tolower(codepage, q->name[i]),
                                         hash);
        q->hash = end_name_hash(hash);

        return 0;
}

static int sfs_ci_compare(const struct dentry *dentry, unsigned int len,
                           const char *name, const struct qstr *a)
{
        struct nls_table *codepage = SFS_SB(dentry->d_inode->i_sb)->local_nls;
        if ((a->len == len) &&
            (nls_strnicmp(codepage, a->name, name, len) == 0)) {
                /*
                 * To preserve case, don't let an existing negative dentry's
                 * case take precedence.  If a is not a negative dentry, this
                 * should have no side effects
                 */
                memcpy((unsigned char *)a->name, name, len);
                return 0;
        }
        return 1;
}

/* No sense hanging on to negative dentries as they are only
in memory - we are not saving anything as we would for network
or disk filesystem */

static int sfs_delete_dentry(const struct dentry *dentry)
{
        return 1;
}

static const struct dentry_operations sfs_ci_dentry_ops = {
/*	.d_revalidate = xxxd_revalidate, Not needed for this type of fs */
	.d_hash = sfs_ci_hash,
	.d_compare = sfs_ci_compare,
	.d_delete = sfs_delete_dentry,
};

/*
 * Lookup the data, if the dentry didn't already exist, it must be
 * negative.  Set d_op to delete negative dentries to save memory
 * (and since it does not help performance for in memory filesystem).
 */
static struct dentry *sfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int len)
{
	if (dentry->d_name.len > NAME_MAX)
		return ERR_PTR(-ENAMETOOLONG);
	dentry->d_op = &sfs_ci_dentry_ops;
	d_add(dentry, NULL);
	return NULL;
}


static struct inode_operations sfs_ci_inode_ops = {
	.lookup		= sfs_lookup,
};

static struct inode *samplefs_get_inode(struct super_block *sb, int mode, dev_t dev)
{
        struct inode * inode = new_inode(sb);
	struct timespec64 ts;
	struct samplefs_sb_info * sfs_sb = SFS_SB(sb);

        if (inode) {
                inode->i_mode = mode;
                inode->i_uid = current->cred->fsuid;
                inode->i_gid = current->cred->fsgid;
                inode->i_blocks = 0;
		ktime_get_ts64(&ts);
                inode->i_atime = inode->i_mtime = inode->i_ctime = ts;
		printk(KERN_INFO "about to set inode ops\n");
                switch (mode & S_IFMT) {
                default:
			init_special_inode(inode, mode, dev);
			break;
/* We are ready to handle files yet */
                case S_IFREG:
			printk(KERN_INFO "file inode\n");
			inode->i_op = &simple_dir_inode_operations;
			break;
                case S_IFDIR:
			printk(KERN_INFO "directory inode sfs_sb: %p\n",sfs_sb);
			if (sfs_sb->flags & SFS_MNT_CASE)
				inode->i_op = &sfs_ci_inode_ops;
			else
	                        inode->i_op = &simple_dir_inode_operations;

                        /* link == 2 (for initial ".." and "." entries) */
                        inode->__i_nlink++;
                        break;
                }
        }
        return inode;
	
}

static int samplefs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct inode * inode;
	struct samplefs_sb_info * sfs_sb;

	sb->s_maxbytes = MAX_LFS_FILESIZE; /* NB: may be too large for mem */
	sb->s_blocksize = PAGE_SIZE;
	sb->s_blocksize_bits = PAGE_SHIFT;
	sb->s_magic = SAMPLEFS_MAGIC;
	sb->s_op = &samplefs_super_ops;
	sb->s_time_gran = 1; /* 1 nanosecond time granularity */


	printk(KERN_INFO "samplefs: fill super\n");

#ifdef CONFIG_SAMPLEFS_DEBUG
	printk(KERN_INFO "samplefs: about to alloc s_fs_info\n");
#endif
	sb->s_fs_info = kzalloc(sizeof(struct samplefs_sb_info),GFP_KERNEL);
	sfs_sb = SFS_SB(sb);
	printk(KERN_INFO "samplefs: super block private info flags: %d\n", sfs_sb->flags);
	if (!sfs_sb) {
		return -ENOMEM;
	}

	inode = samplefs_get_inode(sb, S_IFDIR | 0755, 0);
	if (!inode) {
		kfree(sfs_sb);
		return -ENOMEM;
	}
	
	printk(KERN_INFO "samplefs: about to alloc root inode\n");

	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		iput(inode);
		kfree(sfs_sb);
		return -ENOMEM;
	}
	
	/* below not needed for many fs - but an example of per fs sb data */
	sfs_sb->local_nls = load_nls_default();

	samplefs_parse_mount_options(data, sfs_sb);
	
	/* FS-FILLIN your filesystem specific mount logic/checks here */

	return 0;
}

struct dentry *samplefs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_nodev(fs_type, flags, data, samplefs_fill_super);
}

static struct file_system_type samplefs_fs_type = {
	.owner = THIS_MODULE,
	.name = "samplefs",
	.mount = samplefs_mount,
	.kill_sb = kill_anon_super,
	/*  .fs_flags */
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *proc_fs_samplefs;

static int samplefs_debug_open(struct seq_file *file, void *v)
{
	seq_printf(file, "===================Debug Info====================\n");
	return 0;
}

static int samplefs_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, samplefs_debug_open, NULL);
}

static const struct file_operations samplefs_proc_fops = {
	.owner = THIS_MODULE,
	.read = seq_read,
	.llseek = seq_lseek,
	//.write = seq_write,
	.open = samplefs_proc_open,	
	.release = seq_release,
};

void
sfs_proc_init(void)
{
	proc_fs_samplefs = proc_mkdir("fs/samplefs", NULL);
	if (proc_fs_samplefs == NULL)
		return;

	proc_create("DebugData", 0, proc_fs_samplefs, &samplefs_proc_fops);
}

void
sfs_proc_clean(void)
{
	if (proc_fs_samplefs == NULL)
		return;

	remove_proc_entry("DebugData", proc_fs_samplefs);
	remove_proc_entry("fs/samplefs", NULL);
}
#endif /* CONFIG_PROC_FS */

static int __init init_samplefs_fs(void)
{
	printk(KERN_INFO "init samplefs\n");
#ifdef CONFIG_PROC_FS
	sfs_proc_init();
#endif

	/* some filesystems pass optional parms at load time */
	if (sample_parm > 256) {
		printk(KERN_ERR "sample_parm %d too large, reset to 10\n",
			sample_parm);
		sample_parm = 10;
	}

	return register_filesystem(&samplefs_fs_type);
}

static void __exit exit_samplefs_fs(void)
{
	printk(KERN_INFO "unloading samplefs\n");
#ifdef CONFIG_PROC_FS
	sfs_proc_clean();
#endif
	unregister_filesystem(&samplefs_fs_type);
}

module_init(init_samplefs_fs)
module_exit(exit_samplefs_fs)
MODULE_LICENSE("GPL");
