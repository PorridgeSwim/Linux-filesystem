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
/*
int pantryfs_iterate(struct file *filp, struct dir_context *ctx)
{
	struct buffer_head *tmp_bh, *i_store_bh;
	struct pantryfs_dir_entry *pde;
	unsigned int offset;
	uint64_t block;
	unsigned int pfs_dirent_size;
	struct inode *dir = file_inode(filp);
	struct pantryfs_inode *pfs_inode;

	pfs_dirent_size = sizeof(struct pantryfs_dir_entry);
	if(ctx->pos & (pfs_dirent_size - 1)) {
		printk("Bad f_pos=%08lx for %s:%08lx\n",
					(unsigned long)ctx->pos,
					dir->i_sb->s_id, dir->i_ino);//not the right pos
		return -EINVAL;
	}
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(dir->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode* )(i_store_bh->b_data);
	while(ctx->pos < dir->i_size){//iterate in dir's inodes
		offset = ctx -> pos & (PFS_BLOCK_SIZE - 1);//pos & 111111111111
		block = pfs_inode->data_block_number + (ctx -> pos >> 12);
		tmp_bh = sb_bread(dir->i_sb, block);
		if(!tmp_bh){
			ctx->pos += PFS_BLOCK_SIZE - offset; 
			continue;
		}
		do{
			pde = (struct pantryfs_dir_entry *)(tmp_bh->b_data + offset);
			if(pde -> inode_no) {
				int size = strnlen(pde -> filename, PANTRYFS_FILENAME_BUF_SIZE);
				if(!dir_emit(ctx, pde->filename, size,
							le16_to_cpu(pde->inode_no),
							DT_UNKNOWN)) {
					brelse(tmp_bh);
					return 0;
				}
			}
			offset+= pfs_dirent_size;
			ctx->pos += pfs_dirent_size;
		}while ((offset < PFS_BLOCK_SIZE) && (ctx->pos < dir->i_size));
		brelse(tmp_bh);
	}

	return 0;
}
*/
int pantryfs_iterate(struct file *filp, struct dir_context *ctx)
{
	// return -EPERM;
	// bool moved = false;//moved needed?
	struct buffer_head *tmp_bh, *i_store_bh;
	struct pantryfs_dir_entry *pde;
	struct pantryfs_inode *pfs_inode;
	uint64_t data_block_number;
	struct pantryfs_dir_entry *start;
	struct inode *i_node;
	unsigned long i_ino;

	i_node = file_inode(filp);
	i_ino = i_node->i_ino;//maximum of pos
	i_store_bh = ((struct pantryfs_sb_buffer_heads *)(i_node->i_sb->s_fs_info))->i_store_bh;
	pfs_inode = (struct pantryfs_inode* )(i_store_bh->b_data) + (le64_to_cpu(i_ino)-1); //* sizeof(struct pantryfs_inode));
	data_block_number = pfs_inode->data_block_number;

	tmp_bh = sb_bread(i_node->i_sb, data_block_number);//get the block
	start = (struct pantryfs_dir_entry *)tmp_bh->b_data;
	if (!dir_emit_dots(filp, ctx))
		return 0;
	pr_info("1ctx pos is: %lld\n", ctx->pos);
	while(ctx->pos < PFS_MAX_CHILDREN) {
		pde = start + ctx->pos;
		pr_info("ctx pos is: %lld\n", ctx->pos);
		if(pde == NULL)
			break;
		if(pde->active == 1){
			int size = strnlen(pde -> filename, PANTRYFS_MAX_FILENAME_LENGTH);
			if(!dir_emit(ctx, pde->filename, size,
						le64_to_cpu(pde->inode_no),
						DT_UNKNOWN)) {
				return 0;
			}	
		}
		ctx->pos++;
	}
	// while ((next = next_positive(dentry, p, 1)) != NULL) {
	// 	if (!dir_emit(ctx, next->d_name.name, next->d_name.len,
	// 		      d_inode(next)->i_ino, dt_type(d_inode(next))))
	// 		break;
	// 	moved = true;
	// 	p = &next->d_child;
	// 	ctx->pos++;
	// }
	return 0;
}

// static inline bool dir_emit(struct dir_context *ctx,
// 			    const char *name, int namelen,
// 			    u64 ino, unsigned type)

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
	return NULL;
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
