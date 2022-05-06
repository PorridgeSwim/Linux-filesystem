#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#include "pantryfs_inode.h"
#include "pantryfs_inode_ops.h"
#include "pantryfs_file.h"
#include "pantryfs_file_ops.h"
#include "pantryfs_sb.h"
#include "pantryfs_sb_ops.h"

int pantryfs_iterate(struct file *filp, struct dir_context *ctx)
{
	// bool moved = false;//moved needed?
	struct buffer_head *tmp_bh, *i_store_bh;
	struct pantryfs_dir_entry *pde;
	struct pantryfs_inode *pfs_inode;
	uint64_t data_block_number;
	struct pantryfs_dir_entry *start;
	struct inode *i_node;
	unsigned long i_ino;
	int size;

	//pr_info("begin iterate\n");
	i_node = file_inode(filp);
	i_ino = i_node->i_ino;//maximum of pos
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(i_node->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)(i_store_bh->b_data) + (le64_to_cpu(i_ino)-1);
	data_block_number = pfs_inode->data_block_number;

	tmp_bh = sb_bread(i_node->i_sb, data_block_number);//get the block
	if (!tmp_bh)
		return -ENOENT;

	start = (struct pantryfs_dir_entry *)tmp_bh->b_data;
	if (!dir_emit_dots(filp, ctx))
		goto iter_out;

	pde = start;
	while (ctx->pos < PFS_MAX_CHILDREN) {
		if (pde == NULL)
			break;
		if (pde->active == 1) {
			size = strnlen(pde->filename, PANTRYFS_MAX_FILENAME_LENGTH);
			if (!dir_emit(ctx, pde->filename, size,
						pde->inode_no,
						S_DT(i_node->i_mode))) {
				goto iter_out;
			}
		}
		ctx->pos++;
		pde++;
	}

iter_out:
	brelse(tmp_bh);
	return 0;
}

ssize_t pantryfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	struct buffer_head *i_store_bh;
	struct pantryfs_inode *pfs_inode;
	uint64_t data_block_number;
	struct inode *i_node;
	unsigned long i_ino;
	size_t faillen;
	void *startptr;
	uint64_t f_size;	//file size
	struct buffer_head *bh;

	//pr_info("begin read\n");
	i_node = file_inode(filp);
	f_size = i_node->i_size;

	// read check
	if (len < 0 || *ppos < 0)
		return -EINVAL;
	if (*ppos > f_size)
		return -EPERM;
	if (len > f_size - *ppos)
		len = f_size - *ppos;

	// get pfs inode
	i_ino = i_node->i_ino;
	f_size = i_node->i_size;
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(i_node->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)(i_store_bh->b_data) + (le64_to_cpu(i_ino)-1);

	// cache the target data block
	data_block_number = pfs_inode->data_block_number;
	bh = sb_bread(i_node->i_sb, data_block_number);
	if (!bh)
		return -ENOENT;
	startptr = bh->b_data;

	// read from the data block
	faillen = copy_to_user(buf, startptr + *ppos, len);
	if (faillen == len && len > 0) {
		brelse(bh);
		return -EFAULT;
	}

	// update offset and return
	len -= faillen;
	*ppos += len;
	brelse(bh);
	return len;
}

loff_t pantryfs_llseek(struct file *filp, loff_t offset, int whence)
{
	//pr_info("begin llseek\n");
	return generic_file_llseek(filp, offset, whence);
}

int pantryfs_create(struct inode *parent, struct dentry *dentry, umode_t mode, bool excl)
{
	//struct pantryfs_inode *pfs_new_inode;//needed?
	struct inode *newinode;
	uint32_t new_ino, new_blkno;
	struct pantryfs_super_block *pfs_sb;
	struct pantryfs_sb_buffer_heads *pfs_bh;
	struct pantryfs_inode *tmp_pfs_inode, *par_pfs_inode;
	struct buffer_head *par_bh;
	struct pantryfs_dir_entry *tmp_pfs_dentry;
	uint64_t par_ino;
	uint32_t cnt;
	struct timespec64 ts;

	//pr_info("create called\n");
	ktime_get_coarse_real_ts64(&ts);
	pfs_bh = (struct pantryfs_sb_buffer_heads *)parent->i_sb->s_fs_info;
	pfs_sb = (struct pantryfs_super_block *)pfs_bh->sb_bh->b_data;
	tmp_pfs_inode = (struct pantryfs_inode *)pfs_bh->i_store_bh->b_data;
	//find a new inode no, and set ts bit to 1
	new_ino = 0;
	new_blkno = 0;
	while (IS_SET(pfs_sb->free_inodes, new_ino) && new_ino < PFS_MAX_INODES)
		new_ino++;
	while (IS_SET(pfs_sb->free_data_blocks, new_blkno) && new_blkno < PFS_MAX_INODES)
		new_blkno++;
	if (new_ino >= PFS_MAX_INODES || new_blkno >= PFS_MAX_INODES)
		return -ENOSPC;
	SETBIT(pfs_sb->free_inodes, new_ino);
	SETBIT(pfs_sb->free_data_blocks, new_blkno);

	//find a new inode
	newinode = iget_locked(parent->i_sb, new_ino+1);
	if (!newinode) {
		CLEARBIT(pfs_sb->free_inodes, new_ino);
		CLEARBIT(pfs_sb->free_data_blocks, new_blkno);
		pr_info("l154 no newinode\n");
		return -ENOSPC;
	}
	//initialize new inode,
	inode_init_owner(newinode, parent, mode);
	newinode->i_sb = parent->i_sb;
	newinode->i_mtime = newinode->i_atime = newinode->i_ctime = ts;
	newinode->i_blocks = 1;//right?
	newinode->i_op = &pantryfs_inode_ops;
	newinode->i_fop = &pantryfs_file_ops;
	newinode->i_ino = new_ino+1;
	//mark_inode_dirty(newinode);

	//add new pfs dentry to parent's block
	par_ino = le64_to_cpu(parent->i_ino);
	par_pfs_inode = (struct pantryfs_inode *)parent->i_private;
	par_bh = sb_bread(parent->i_sb, le64_to_cpu(par_pfs_inode->data_block_number));
	tmp_pfs_dentry = (struct pantryfs_dir_entry *)(par_bh->b_data);
	for (cnt = 0; cnt < PFS_MAX_CHILDREN; cnt++) {
		if ((!tmp_pfs_dentry) || (tmp_pfs_dentry)->active == 0) {
			//find an empty or deactive pfs dentry
			tmp_pfs_dentry->inode_no = new_ino+1;
			strncpy(tmp_pfs_dentry->filename, dentry->d_name.name,
					sizeof(tmp_pfs_dentry->filename));
			tmp_pfs_dentry->active = 1;
			break;
		}
		tmp_pfs_dentry++;
	}
	if (cnt >= PFS_MAX_CHILDREN) {
		CLEARBIT(pfs_sb->free_inodes, new_ino);
		CLEARBIT(pfs_sb->free_data_blocks, new_blkno);
		discard_new_inode(newinode);
		pr_info("l187 no more child\n");
		return -ENOSPC;
	}

	tmp_pfs_inode += new_ino;
	tmp_pfs_inode->mode = mode;
	tmp_pfs_inode->uid = newinode->i_uid.val;
	tmp_pfs_inode->gid = newinode->i_gid.val;
	tmp_pfs_inode->i_atime = tmp_pfs_inode->i_ctime = tmp_pfs_inode->i_mtime = ts;
	tmp_pfs_inode->nlink = 1;
	tmp_pfs_inode->data_block_number = new_blkno+2;
	tmp_pfs_inode->file_size = 0;

	newinode->i_private = (void *)(struct pantryfs_inode *)tmp_pfs_inode;
	d_instantiate(dentry, newinode);
	unlock_new_inode(newinode);
	mark_buffer_dirty(par_bh);
	mark_buffer_dirty(pfs_bh->i_store_bh);
	sync_dirty_buffer(pfs_bh->i_store_bh);
	brelse(par_bh);
	pr_info("new inode pfsinode pfsdentry created!\n");
	return 0;
}

int pantryfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct buffer_head *bh, *i_store_bh;
	struct pantryfs_dir_entry *pfs_de;
	struct pantryfs_inode *pfs_parent, *pfs_inode;
	uint64_t count = 0;
	const unsigned char *name;
	struct pantryfs_sb_buffer_heads *info = (struct pantryfs_sb_buffer_heads *)
						inode->i_sb->s_fs_info;


	pr_info("unlink begin\n");
	pr_info("unlink reference count is %u\n", inode->i_count.counter);
	if (!dir || !dentry)
		return -EINVAL;
	if (!(dir->i_mode & S_IFDIR))
		return -EINVAL;
	name = dentry->d_name.name;
	i_store_bh = info->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)inode->i_private;
	pfs_parent = (struct pantryfs_inode *)dir->i_private;
	if (!pfs_parent)
		return -ENOENT;
	bh = sb_bread(dir->i_sb, le64_to_cpu(pfs_parent->data_block_number));
	if (!bh)
		return -ENOENT;
	pfs_de = (struct pantryfs_dir_entry *)(bh->b_data);//start of pfs dentries
	while (pfs_de && (count * sizeof(struct pantryfs_dir_entry) < PFS_BLOCK_SIZE)) {
		if ((!strcmp(pfs_de->filename, name)) && (pfs_de->active == 1))
			goto unlink_out;
		pfs_de++;
		count++;
	}
	brelse(bh);
	return -ENOENT;

unlink_out:
	pfs_de->active = 0;
	/*
	 * if (!inode->i_nlink) {
		pr_err("unlinking non-existent file %s:%lu (nlink=%d)\n",
					inode->i_sb->s_id, inode->i_ino,
					inode->i_nlink);
		set_nlink(inode, 1);
	}
	*/
	inode_dec_link_count(inode);
	//d_delete(dentry);

	pantryfs_write_inode(inode, NULL);
	mark_buffer_dirty(bh);
	brelse(bh);
	d_drop(dentry);
	pr_info("unlink end referenve count %u\n", inode->i_count.counter);

	return 0;
}

int pantryfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct buffer_head *i_store_bh;
	struct pantryfs_inode *pfs_inode;
	struct pantryfs_sb_buffer_heads *bhs;

	bhs = (struct pantryfs_sb_buffer_heads *)(inode->i_sb->s_fs_info);
	i_store_bh = bhs->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)inode->i_private;
	pfs_inode->mode = inode->i_mode;
	pfs_inode->uid = inode->i_uid.val;
	pfs_inode->gid = inode->i_gid.val;
	pfs_inode->i_atime = inode->i_atime;
	pfs_inode->i_mtime = inode->i_mtime;
	pfs_inode->i_ctime = inode->i_ctime;
	pfs_inode->nlink = inode->i_nlink;
	pfs_inode->file_size = inode->i_size;

	mark_buffer_dirty(i_store_bh);
	sync_dirty_buffer(i_store_bh);
	return 0;
}

void pantryfs_evict_inode(struct inode *inode)
{
	struct pantryfs_inode *pfs_inode;
	struct buffer_head *i_store_bh, *sb_bh, *bh;
	struct pantryfs_sb_buffer_heads *bhs;
	unsigned long ino;
	struct pantryfs_super_block *pfs_sb;
	unsigned int count;

	pr_info("begin evict reference count %u\n", inode->i_count.counter);
	if (!inode)
		return;

	bhs = (struct pantryfs_sb_buffer_heads *)(inode->i_sb->s_fs_info);
	i_store_bh = bhs->i_store_bh;
	sb_bh = bhs->sb_bh;
	pfs_sb = (struct pantryfs_super_block *)sb_bh->b_data;

	/* Required to be called by VFS. If not called, evict() will BUG out.*/
	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);

	if (inode->i_nlink || inode->i_count.counter)
		return;

	count = (int)inode->i_count.counter;
	pr_info("evict reference count is %u\n", count);
	ino = inode->i_ino;
	pfs_inode = (struct pantryfs_inode *)inode->i_private;
	bh = sb_bread(inode->i_sb, pfs_inode->data_block_number);
	CLEARBIT(pfs_sb->free_data_blocks, pfs_inode->data_block_number - 2);
	CLEARBIT(pfs_sb->free_inodes, ino - 1);
	//memset(pfs_inode, 0, sizeof(struct pantryfs_inode));
	//memset(bh, 0, PFS_BLOCK_SIZE);
	mark_buffer_dirty(i_store_bh);
	sync_dirty_buffer(i_store_bh);
	mark_buffer_dirty(sb_bh);
	sync_dirty_buffer(sb_bh);
	//mark_buffer_dirty(bh);
	//sync_dirty_buffer(bh);
	//brelse(bh);
	pr_info("finish evict\n");
}

int pantryfs_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	pr_info("fsync\n");
	return generic_file_fsync(filp, start, end, datasync);
}

ssize_t pantryfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct buffer_head *i_store_bh;
	struct pantryfs_inode *pfs_inode;
	uint64_t data_block_number;
	struct inode *i_node;
	unsigned long i_ino;
	size_t faillen = 0;
	void *startptr;
	uint64_t f_size;	//file size
	struct buffer_head *bh;
	int ret;

	pr_info("begin write\n");
	i_node = file_inode(filp);

	// write check
	if (*ppos < 0 || len <= 0)
		return -EINVAL;
	if (filp->f_flags & O_APPEND)
		*ppos = i_size_read(i_node);
	if (*ppos >= PFS_BLOCK_SIZE)
		return -EFBIG;
	if (len > PFS_BLOCK_SIZE - *ppos)
		len = PFS_BLOCK_SIZE - *ppos;

	// get pfs inode
	i_ino = i_node->i_ino;
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(i_node->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)(i_store_bh->b_data) + (le64_to_cpu(i_ino)-1);
	f_size = pfs_inode->file_size;

	// cache the target data block on pfs
	data_block_number = pfs_inode->data_block_number;
	bh = sb_bread(i_node->i_sb, data_block_number);
	if (!bh)
		return -ENOENT;
	startptr = bh->b_data;

	// write, change offset, change return value
	faillen = copy_from_user(startptr + *ppos, buf, len);
	if (faillen == len) {
		ret = -EFAULT;
		goto write_out;
	}
	len -= faillen;
	*ppos += len;

	// update inode offset
	i_node->i_size = *ppos;
	ret = len;

write_out:
	// update pfs inode
	mark_buffer_dirty(bh);
	pantryfs_write_inode(i_node, NULL);
	brelse(bh);
	return ret;
}

struct dentry *pantryfs_lookup(struct inode *parent, struct dentry *child_dentry,
		unsigned int flags)
{
	struct pantryfs_inode *pfs_inodes, *pfs_parent, *pfs_child;
	struct buffer_head *bh;
	struct pantryfs_dir_entry *pfs_de;
	struct inode *child;

	struct pantryfs_sb_buffer_heads *info;
	const unsigned char *name;
	uint64_t count = 0;

	//pr_info("begin lookup\n");
	// loopup check
	if (!parent)
		return ERR_PTR(-EINVAL);
	info = parent->i_sb->s_fs_info;
	if (!child_dentry)
		return ERR_PTR(-EINVAL);
	if (child_dentry->d_name.len > PANTRYFS_MAX_FILENAME_LENGTH)
		return ERR_PTR(-ENAMETOOLONG);

	// cache parent's data block
	name = child_dentry->d_name.name;
	pfs_inodes = (struct pantryfs_inode *)(info->i_store_bh->b_data);//start of inodes
	pfs_parent = pfs_inodes + le64_to_cpu(parent->i_ino) - 1;//parent's inode
	if (!pfs_parent)
		return ERR_PTR(-ENOENT);//no parent inode
	bh = sb_bread(parent->i_sb, le64_to_cpu(pfs_parent->data_block_number));//parent's block

	if (bh) {
		pfs_de = (struct pantryfs_dir_entry *)(bh->b_data);//start of pfs dentries

		// loop until find the same name
		while (pfs_de && (count * sizeof(struct pantryfs_dir_entry) < PFS_BLOCK_SIZE)) {
			if ((!strcmp(pfs_de->filename, name)) && (pfs_de->active == 1))
				goto retrieve;
			pfs_de++;
			count++;
		}
		d_add(child_dentry, NULL);
		brelse(bh);
		bh = NULL;
		return NULL;
	}
	bh = NULL;
	// pr_info("look up return 365\n");
	return ERR_PTR(-ENOENT);

retrieve:
	// get child pfs inode
	pfs_child = pfs_inodes +  pfs_de->inode_no - 1;//pfs inode
	if (!pfs_child) {
		brelse(bh);
		// pr_info("look up return 373\n");
		return ERR_PTR(-ENOENT);
	}

	// create vfs inode for child
	child = iget_locked(parent->i_sb, pfs_de->inode_no); //VFS inode
	if (!child) {
		brelse(bh);
		// pr_info("look up return 381\n");
		return ERR_PTR(-ENOENT);
	}
	if (child->i_state && I_NEW) {
		// synchronize statistic
		child->i_mode = pfs_child->mode;
		child->i_op = &pantryfs_inode_ops;
		if (child->i_mode & S_IFDIR) {
			child->i_fop = &pantryfs_dir_ops;
		} else if (child->i_mode & S_IFREG) {
			child->i_fop = &pantryfs_file_ops;
		} else {
			unlock_new_inode(child);
			brelse(bh);
			return ERR_PTR(-EINVAL);
		}
		child->i_size = pfs_child->file_size;
		child->i_private = (void *)(struct pantryfs_inode *) pfs_child;
		child->i_ino = pfs_de->inode_no;
		child->i_sb = parent->i_sb;
		child->i_atime = pfs_child->i_atime;
		child->i_mtime = pfs_child->i_mtime;
		child->i_ctime = pfs_child->i_ctime;
		child->i_uid.val = pfs_child->uid;
		child->i_gid.val = pfs_child->gid;
		set_nlink(child, pfs_child->nlink);
		child->i_blocks = (child->i_size + 512 - 1) >> 9;

		unlock_new_inode(child);
	}

	// associate this inode with dentry
	d_add(child_dentry, child);
	brelse(bh);
	bh = NULL;
	return NULL;
}

int pantryfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *newinode;
	struct pantryfs_inode *tmp_pfs_inode, *par_pfs_inode;
	struct pantryfs_super_block *pfs_sb;
	struct pantryfs_sb_buffer_heads *pfs_bh;
	struct buffer_head *par_bh;
	struct pantryfs_dir_entry *tmp_pfs_dentry;
	uint32_t new_ino = 0, new_blkno = 0;
	uint64_t par_ino;
	struct timespec64 ts;
	int cnt;

	mode |= S_IFDIR | 0777;

	if (S_ISDIR(mode))
		pr_info("mode is dir\n");

	pr_info("make dir start\n");
	// return -EPERM;

	ktime_get_coarse_real_ts64(&ts);
	inode_inc_link_count(dir);

	// find empty inode and datablock
	pfs_bh = (struct pantryfs_sb_buffer_heads *)dir->i_sb->s_fs_info;
	pfs_sb = (struct pantryfs_super_block *)pfs_bh->sb_bh->b_data;
	tmp_pfs_inode = (struct pantryfs_inode *)pfs_bh->i_store_bh->b_data; // introduce crash

	//find a new inode no, and set ts bit to 1
	new_ino = 0;
	new_blkno = 0;
	while (IS_SET(pfs_sb->free_inodes, new_ino) && new_ino < PFS_MAX_INODES)
		new_ino++;
	while (IS_SET(pfs_sb->free_data_blocks, new_blkno) && new_blkno < PFS_MAX_INODES)
		new_blkno++;
	if (new_ino > PFS_MAX_INODES || new_blkno > PFS_MAX_INODES) {
		inode_dec_link_count(dir);
		pr_info("no empty block\n");
		return -ENOSPC;
	}
	SETBIT(pfs_sb->free_inodes, new_ino);
	SETBIT(pfs_sb->free_data_blocks, new_blkno);
	pr_info("find empty inode %u\n", new_ino);
	pr_info("find empty block %u\n", new_blkno);

	// create new inode in VFS
	newinode = iget_locked(dir->i_sb, new_ino+1);
	if (!newinode) {
		CLEARBIT(pfs_sb->free_inodes, new_ino);
		CLEARBIT(pfs_sb->free_data_blocks, new_blkno);
		inode_dec_link_count(dir);
		pr_info("no new inode\n");
		return -ENOSPC;
	}

	//initialize new inode in VFS
	inode_init_owner(newinode, dir, mode); // id, mode
	newinode->i_sb = dir->i_sb;
	newinode->i_mtime = newinode->i_atime = newinode->i_ctime = ts;
	newinode->i_blocks = 8; //right?
	newinode->i_op = &pantryfs_inode_ops;
	newinode->i_fop = &pantryfs_dir_ops;
	newinode->i_ino = new_ino + 1;
	newinode->i_size = PFS_BLOCK_SIZE;
	set_nlink(newinode, 2);
	// newinode->i_nlink = 2;
	// newinode->i_mode = mode;

	pr_info("set dentry\n");
	//add new pfs dentry to parent's block
	par_ino = le64_to_cpu(dir->i_ino);
	par_pfs_inode = (struct pantryfs_inode *)dir->i_private;
	par_bh = sb_bread(dir->i_sb, le64_to_cpu(par_pfs_inode->data_block_number));
	tmp_pfs_dentry = (struct pantryfs_dir_entry *)(par_bh->b_data);
	for (cnt = 0; cnt < PFS_MAX_CHILDREN; cnt++) {
		if ((!tmp_pfs_dentry) || (tmp_pfs_dentry)->active == 0) {
			tmp_pfs_dentry->inode_no = new_ino+1;
			strncpy(tmp_pfs_dentry->filename, dentry->d_name.name,
						sizeof(tmp_pfs_dentry->filename));
			tmp_pfs_dentry->active = 1;
			break;
		}
		tmp_pfs_dentry++;
	}

	if (cnt >= PFS_MAX_CHILDREN) {
		CLEARBIT(pfs_sb->free_inodes, new_ino);
		CLEARBIT(pfs_sb->free_data_blocks, new_blkno);
		discard_new_inode(newinode);
		inode_dec_link_count(dir);
		return -ENOSPC;
	}
	pr_info("set pfs_inode\n");

	tmp_pfs_inode += new_ino;
	tmp_pfs_inode->mode = mode;
	tmp_pfs_inode->uid = newinode->i_uid.val;
	tmp_pfs_inode->gid = newinode->i_gid.val;
	tmp_pfs_inode->i_atime = tmp_pfs_inode->i_ctime = tmp_pfs_inode->i_mtime = ts;
	tmp_pfs_inode->nlink = 2;
	tmp_pfs_inode->data_block_number = new_blkno + 2;
	tmp_pfs_inode->file_size = PFS_BLOCK_SIZE;

	newinode->i_private = (void *)(struct pantryfs_inode *)tmp_pfs_inode;
	d_instantiate(dentry, newinode);
	unlock_new_inode(newinode);
	mark_buffer_dirty(par_bh);
	mark_buffer_dirty(pfs_bh->i_store_bh);
	sync_dirty_buffer(pfs_bh->i_store_bh);
	brelse(par_bh);
	pr_info("new inode pfsinode pfsdentry created!\n");
	return 0;

}

int pantryfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = d_inode(dentry);
	struct pantryfs_inode *pfs_inode;
	struct buffer_head *bh;
	struct pantryfs_dir_entry *pfs_de;
	int cnt;

	pr_info("rmdir begin\n");
	if (!dir || !dentry)
		return -EINVAL;

	pfs_inode = (struct pantryfs_inode *)inode->i_private;
	bh = sb_bread(dir->i_sb, le64_to_cpu(pfs_inode->data_block_number));
	if (!bh)
		return -ENOENT;

	pfs_de = (struct pantryfs_dir_entry *)(bh->b_data);
	for (cnt = 0; cnt < PFS_MAX_CHILDREN; cnt++) {
		if ((pfs_de) && (pfs_de)->active == 1) {
			brelse(bh);
			pr_info("not empty\n");
			return -ENOTEMPTY;
		}
		pfs_de++;
	}

	drop_nlink(inode);
	pantryfs_unlink(dir, dentry);
	drop_nlink(dir);

	return 0;

}

int pantryfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry)
{
	return -EPERM;
}

int pantryfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	return -EPERM;
}

const char *pantryfs_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *done)
{
	return ERR_PTR(-EPERM);
}

/**
 * Called by VFS to free an inode. free_inode_nonrcu() must be called to free
 * the inode in the default manner.
 *
 * @inode:	The inode that will be free'd by VFS.
 */
void pantryfs_free_inode(struct inode *inode)
{
	free_inode_nonrcu(inode);
}

int pantryfs_fill_super(struct super_block *sb, void *data, int silent)//what is data?
{
	struct pantryfs_super_block *pfs_sb;	// pfs_superblock
	struct pantryfs_inode *pfs_inode;
	struct pantryfs_sb_buffer_heads *two_bufferheads;
	struct buffer_head *sb_bh, *i_store_bh;
	struct inode *root_inode;
	struct dentry *root_dentry;

	//pr_info("fill super\n");
	// cache sb_bh and i_store_bh
	sb_bh = sb_bread(sb, 0);
	pfs_sb = (struct pantryfs_super_block *) (sb_bh->b_data);
	if (le32_to_cpu(pfs_sb->magic) != PANTRYFS_MAGIC_NUMBER) {
		brelse(sb_bh);
		return -EPERM;
	}
	i_store_bh = sb_bread(sb, 1);
	pfs_inode = (struct pantryfs_inode *) (i_store_bh->b_data);
	two_bufferheads = kmalloc(sizeof(struct pantryfs_sb_buffer_heads), GFP_KERNEL);
	if (!two_bufferheads)
		return -ENOMEM;
	two_bufferheads->sb_bh = sb_bh;//reads a specified block and returns the bh
	two_bufferheads->i_store_bh = i_store_bh;
	/*sb defination*/

	sb_set_blocksize(sb, PFS_BLOCK_SIZE);//1. set s_blocksize and s_blocksize_bits
	sb->s_fs_info = (void *)two_bufferheads; //2.buffer head
	sb->s_magic = PANTRYFS_MAGIC_NUMBER; //3.magic number
	sb->s_maxbytes = PFS_BLOCK_SIZE;
	//4.allocate inode bitmap?
	sb->s_op = &pantryfs_sb_ops;//5. initialize op
	root_inode = iget_locked(sb, PANTRYFS_ROOT_INODE_NUMBER); //5.invoke inode cache
	//-- obtain an inode from a mounted file system
	if (!root_inode) {
		sb->s_fs_info = NULL;
		kfree(two_bufferheads);
		return -EINVAL;
	}
	root_inode->i_mode = S_IFDIR | 0777;//inode or pantryfs inode?
	root_inode->i_op = &pantryfs_inode_ops;
	root_inode->i_fop = &pantryfs_dir_ops;
	root_inode->i_private = i_store_bh->b_data;
	root_inode->i_sb = sb;
	root_inode->i_size = PFS_BLOCK_SIZE;
	root_inode->i_atime = pfs_inode->i_atime;
	root_inode->i_mtime = pfs_inode->i_mtime;
	root_inode->i_ctime = pfs_inode->i_ctime;
	root_inode->i_uid.val = pfs_inode->uid;
	root_inode->i_gid.val = pfs_inode->gid;
	set_nlink(root_inode, pfs_inode->nlink);
	root_inode->i_blocks = (root_inode->i_size + 512 - 1) >> 9;
	unlock_new_inode(root_inode);

	root_dentry = d_obtain_root(root_inode);//5. allocate a dentry
	if (!root_dentry) {
		sb->s_fs_info = NULL;
		kfree(two_bufferheads);
		return -ENOMEM;
	}
	sb->s_root = root_dentry;

	return 0;
}

static struct dentry *pantryfs_mount(struct file_system_type *fs_type, int flags,
		const char *dev_name, void *data)
{
	struct dentry *ret;

	/* mount_bdev is "mount block device". */
	ret = mount_bdev(fs_type, flags, dev_name, data, pantryfs_fill_super);

	if (IS_ERR(ret))
		pr_err("Error mounting mypantryfs");
	else
		pr_info("Mounted mypantryfs on [%s]\n", dev_name);

	return ret;
}

static void pantryfs_kill_superblock(struct super_block *sb)
{
	struct s_fs_info *info;

	info = sb->s_fs_info;
	brelse(((struct pantryfs_sb_buffer_heads *)sb->s_fs_info)->sb_bh);
	brelse(((struct pantryfs_sb_buffer_heads *)sb->s_fs_info)->i_store_bh);
	kill_block_super(sb);
	kfree(info);
	pr_info("mypantryfs superblock destroyed. Unmount successful.\n");
}

struct file_system_type pantryfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "mypantryfs",
	.mount = pantryfs_mount,
	.kill_sb = pantryfs_kill_superblock,
};

static int pantryfs_init(void)
{
	int ret;

	ret = register_filesystem(&pantryfs_fs_type);
	if (likely(ret == 0))
		pr_info("Successfully registered mypantryfs\n");
	else
		pr_err("Failed to register mypantryfs. Error:[%d]", ret);

	return ret;
}

static void pantryfs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&pantryfs_fs_type);

	if (likely(ret == 0))
		pr_info("Successfully unregistered mypantryfs\n");
	else
		pr_err("Failed to unregister mypantryfs. Error:[%d]", ret);
}

module_init(pantryfs_init);
module_exit(pantryfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group N");