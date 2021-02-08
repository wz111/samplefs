/*
 *   fs/samplefs/file.c
 *
 *   Copyright (C) International Business Machines  Corp., 2006
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   Sample File System
 *
 *   Primitive example to show how to create a Linux filesystem module
 *
 *   File struct (file instance) related functions
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

struct address_space_operations sfs_aops = {
	.readpage       = simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end
};

struct file_operations sfs_file_operations = {
	//.read           = new_sync_read,
	.read_iter	= generic_file_read_iter,
	//.write          = new_sync_write,
	.write_iter	= generic_file_write_iter,
	.mmap           = generic_file_mmap,
	.fsync          = generic_file_fsync,
	.llseek         = generic_file_llseek,
};

