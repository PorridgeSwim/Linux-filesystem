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

	i_node = file_inode(filp);
	i_ino = i_node->i_ino;//maximum of pos
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(i_node->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode *)(i_store_bh->b_data) + (le64_to_cpu(i_ino)-1);
	data_block_number = pfs_inode->data_block_number;

	tmp_bh = sb_bread(i_node->i_sb, data_block_number);//get the block
	start = (struct pantryfs_dir_entry *)tmp_bh->b_data;
	if (!dir_emit_dots(filp, ctx))
		return 0;
	// ctx-> pos -= 2; //why can't do this?
	// pr_info("before loop ctx pos is: %lld\n", ctx->pos);
	pde = start;
	while (ctx->pos < PFS_MAX_CHILDREN) {
		// pr_info("ctx pos is: %lld\n", ctx->pos);
		if (pde == NULL)
			break;
		if (pde->active == 1) {
			size = strnlen(pde->filename, PANTRYFS_MAX_FILENAME_LENGTH);
			if (!dir_emit(ctx, pde->filename, size,
						pde->inode_no,
						DT_UNKNOWN)) {
				return 0;
			}
		}
		ctx->pos++;
		pde++;
	}
	return 0;
}

ssize_t pantryfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	return -EPERM;
}

loff_t pantryfs_llseek(struct file *filp, loff_t offset, int whence)
{
	return -EPERM;
}

int pantryfs_create(struct inode *parent, struct dentry *dentry, umode_t mode, bool excl)
{
	return -EPERM;
}

int pantryfs_unlink(struct inode *dir, struct dentry *dentry)
{
	return -EPERM;
}

int pantryfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return -EPERM;
}

void pantryfs_evict_inode(struct inode *inode)
{
	/* Required to be called by VFS. If not called, evict() will BUG out.*/
	truncate_inode_pages_final(&inode->i_data);
	clear_inode(inode);
}

int pantryfs_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	return -EPERM;
}

ssize_t pantryfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	return -EPERM;
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

	if (!parent)
		return ERR_PTR(-EINVAL);
	info = parent->i_sb->s_fs_info;
	if (!child_dentry)
		return ERR_PTR(-EINVAL);
	if (child_dentry->d_name.len > PANTRYFS_MAX_FILENAME_LENGTH)
		return ERR_PTR(-ENAMETOOLONG);
	name = child_dentry->d_name.name;
	pfs_inodes = (struct pantryfs_inode *)(info->i_store_bh->b_data);//start of inodes
	// pfs_parent = pfs_inodes + le64_to_cpu(child_dentry->inode_no) - 1;//why child dentry?
	pfs_parent = pfs_inodes + le64_to_cpu(parent->i_ino) - 1;//parent's inode
	if (!pfs_parent)
		return ERR_PTR(-ENOENT);//no parent inode
	bh = sb_bread(parent->i_sb, le64_to_cpu(pfs_parent->data_block_number));//parent's block
	if (bh) {
		// pr_info();
		pfs_de = (struct pantryfs_dir_entry *)(bh->b_data);//start of pfs dentries
		while (pfs_de && (count * sizeof(struct pantryfs_dir_entry) < PFS_BLOCK_SIZE)) {
			if ((!strcmp(pfs_de->filename, name)) && (pfs_de->active == 1)) {
				brelse(bh);
				goto retrieve;
			}
			pfs_de++;
			count++;
		}
		brelse(bh);
		return ERR_PTR(-ENOENT);//no dentry
	}
	bh = NULL;
	return ERR_PTR(-ENOENT);

retrieve:
	pfs_child = pfs_inodes +  pfs_de->inode_no - 1;//pfs inode
	if (!pfs_child)
		return ERR_PTR(-ENOENT);
	child = iget_locked(parent->i_sb, pfs_de->inode_no); //VFS inode
	if (!child)
		return ERR_PTR(-ENOENT);
	if (child->i_state && I_NEW) {
		child->i_mode = pfs_child->mode;
		child->i_op = &pantryfs_inode_ops;
		if (child->i_mode & S_IFDIR) {
			child->i_fop = &pantryfs_dir_ops;
		} else if (child->i_mode & S_IFREG) {
			child->i_fop = &pantryfs_file_ops;
		} else {
			unlock_new_inode(child);
			return ERR_PTR(-EINVAL);
		}
		child->i_private = (void *)(struct pantryfs_inode *) pfs_child;
		child->i_ino = pfs_de->inode_no;
		child->i_sb = parent->i_sb;
		// pr_info("l157\n");
		unlock_new_inode(child);
	}
	// pr_info("i_node: %llu\n", pfs_de->inode_no);
	// pr_info("ci_node: %lu\n", child->i_ino);
	// pr_info("159\n");
	d_add(child_dentry, child);
	bh = NULL;
	return 0;
}

int pantryfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return -EPERM;
}

int pantryfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	return -EPERM;
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
	struct pantryfs_sb_buffer_heads *two_bufferheads;
	struct buffer_head *sb_bh, *i_store_bh;
	struct inode *root_inode;
	struct dentry *root_dentry;

	sb_bh = sb_bread(sb, 0);
	pfs_sb = (struct pantryfs_super_block *) (sb_bh->b_data);
	if (le32_to_cpu(pfs_sb->magic) != PANTRYFS_MAGIC_NUMBER) {
		brelse(sb_bh);
		return -EPERM;
	}
	i_store_bh = sb_bread(sb, 1);
	two_bufferheads = kmalloc(sizeof(struct pantryfs_sb_buffer_heads), GFP_KERNEL);
	if (!two_bufferheads)
		return -ENOMEM;

	two_bufferheads->sb_bh = sb_bh;//reads a specified block and returns the bh
	two_bufferheads->i_store_bh = i_store_bh;
	/*sb defination*/

	sb_set_blocksize(sb, PFS_BLOCK_SIZE);//1. set s_blocksize and s_blocksize_bits
	sb->s_fs_info = (void *)two_bufferheads; //2.buffer head
	sb->s_magic = PANTRYFS_MAGIC_NUMBER; //3.magic number
	// i_store_bh//4.allocate inode bitmap?
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
	root_inode->i_sb = sb;//?
	unlock_new_inode(root_inode);
	root_dentry = d_obtain_root(root_inode);//5. allocate a dentry
	if (!root_dentry) {
		sb->s_fs_info = NULL;
		kfree(two_bufferheads);
		return -ENOMEM;
	}
	sb->s_root = root_dentry;
	//6. go through all inodes...
	//7. not read only-- mark sb buffer dirty and set s_dirty
	// return -EPERM;

	brelse(sb_bh);
	brelse(i_store_bh);
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
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;

	kill_block_super(sb);
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
