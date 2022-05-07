This file should contain:

-You Zhou yz3883, Aoxue Wei aw3389, Panyu Gao pg2676
-HW8
-Description for each part

The description should indicate whether your solution for the part is working
or not. You may also want to include anything else you would like to
communicate to the grader, such as extra functionality you implemented or how
you tried to fix your non-working code.

Part1:
We can directly use the bitmap to decide a inode is active or not, so there
is no need to zero out the rest of block. For root directory block, it stores
directory entries. And we use the active member to decide whether a dentry is
active or not, zeroing out the rest of block is quicker way to make all rest
dentries inative than set their active member one by one. Our extension of
format_disk_as_pantryfs.c works well.

Part2:
Our solution is working. We mainly completed the 'pantryfs_fill_super' function
in this part. This function is used for fill the super block so that we can set
up out pantry file system. After implementing this function, we are able to mount
and unmount pantryfs.

Part3:
Our solution is working. We completed the 'pantryfs_iterate' function in this part.
After implementing this function, we can use 'ls' and 'ls -a' to list the member of
the root directory.

Part4:
Our solution is working. We completed the 'pantryfs_lookup' function in this part.
After implementing this function. We can us 'ls' to list the subdirectory of the root.

Part5:
Our solution is working. We completed the 'pantryfs_read' and 'pantryfs_llseek' function
in this part. We can use 'cat' and 'vim' to read the contents of a regular file here.

Part6:
Our solution is working. The information of the output of 'ls -al' and 'stat' before this
part is not correct. We modified the 'pantryfs_fill_super' and 'pantryfs_lookup' function
in this part to update the information of the VFS inode, so that the 'ls' and 'stat'
commands can show correct information such as time, size, nlink and so forth. We also
create a new format program to generate more directory structure and file with different
types. Our solution passed all the tests.

Part7:
Our solution is working. We completed 'pantryfs_write' in this part. We can use 'echo' or
'vim' to modify the contents of a file after implementing this function. And I try to use
the generic fsync function here, it works well.

Part8:
Our solution is working. We mainly completed 'pantryfs_create' in this part. We can use
'vim' or 'touch' to create new files now. It should be noted that only when the
'pantryfs_lookup' function returns NULL, 'pantryfs_create' will be called and create
the new file.

Part9:
After debuging, I noticed that we don't need to change the reference count
in unlink() because system will do it for us. But decreaseing nlink is
necessary. And I didn't actually clear out any memory in this part. For
removing dentry, I just set it to be inactive. For inode and datablock, I
just set their bitmap to be 0 to indicate that they can be overwritten.

Part10:
Our solution is working. We completed 'pantryfs_mkdir' and 'pantryfs_rmdir'
in this part. 'pantryfs_mkdir' is almost the same 'pantryfs_create', we only
need to change the mode to directory and the file operations to directory
operations. 'pantryfs_rmdir' is almost the same as delete a file, we just
added an empty-checking part and called 'pantryfs_unlink' function here.

Part11:
Hard link only add a dentry rather than a inode, so it is different from
creating file. But we can still learn from unlink(), we are just doing the
opposite thing here. In this part, I encounter a big bug because I directly
casted the buffer head rather than its b_data, but it works well finally.

Part12:
Our solution is working. We completed 'pantryfs_symlink' function in this part.
It is almost the same as 'pantryfs_create'. After implementing this function,
we are able to create symbolic links. The only things need to notice are the
mode of inode is S_IFLNK, and free inode->ilink in free_inode(). All other
things are similar to create a file.
