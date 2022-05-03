#ifndef __PANTRY_FS_INODE_OPS_H__
#define __PANTRY_FS_INODE_OPS_H__
struct dentry *pantryfs_lookup(struct inode *parent, struct dentry *child_dentry,
			unsigned int flags)
{
	struct pantryfs_inode *pfs_inodes, *pfs_parent, *pfs_child;
	struct buffer_head *bh;
	struct pantryfs_dir_entry *pfs_de;
	struct pantryfs_sb_buffer_heads *info = parent->i_sb->s_fs_info;
	const unsigned char *name = child_dentry->d_name.name;
	int count = 0;
	struct inode *child;

	if (child_dentry->d_name.len > PANTRYFS_MAX_FILENAME_LENGTH)
		return ERR_PTR(-ENAMETOOLONG);

	pfs_inodes = ((struct pantryfs_inode *)(info->i_store_bh->b_data))->data_block_number;
	pfs_parent = pfs_inodes + le64_to_cpu(child_dentry->inode_no) - 1;

	bh = sb_bread(parent->i_sb, le64_to_cpu(target_inode->data_block_number));
	if (bh) {
		pfs_de = (struct pantryfs_dir_entry *)(bh->b_data);
		while (pfs_de && count * sizeof(struct pantryfs_dir_entry) < PFS_BLOCK_SIZE) {
			if (strcmp(pfs_de->filename, name) && le8_to_cpu(pfs_de->active)) {
				brelse(bh);
				goto retrieve;
			}
			pfs_de++;
			count ++;
		}
		return ERR_PTE(-ENOENT);
	} else {
		brelse(bh);
		bh = NULL;
		return ERR_PTE(-ENOENT);
	}

retrieve:
	pfs_child = pfs_inodes + pfs_de->inode_no -1;
	child->i_mode = pfs_child->mode;
	child->i_op = &pantryfs_inode_ops;
	child->i_private = (void *)(struct pantryfs_inode *) pfs_child;

	d_add(pfs_de, child);


};
int pantryfs_create(struct inode *parent, struct dentry *dentry, umode_t mode, bool excl);
int pantryfs_unlink(struct inode *dir, struct dentry *dentry);
int pantryfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int pantryfs_rmdir(struct inode *dir, struct dentry *dentry);
int pantryfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *dentry);
int pantryfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
const char *pantryfs_get_link(struct dentry *dentry, struct inode *inode,
	struct delayed_call *done);

const struct inode_operations pantryfs_inode_ops = {
	.lookup = pantryfs_lookup,
	.create = pantryfs_create,
	.unlink = pantryfs_unlink,
	.mkdir = pantryfs_mkdir,
	.rmdir = pantryfs_rmdir,
	.link = pantryfs_link,
	.symlink = pantryfs_symlink,
};

const struct inode_operations pantryfs_symlink_inode_ops = {
	.get_link = pantryfs_get_link
};
#endif /* ifndef __PANTRY_FS_INODE_OPS_H__ */
