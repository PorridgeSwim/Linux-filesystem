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

Part3:

Part4:


Part9:
After debuging, I noticed that we don't need to change the reference count
in unlink() because system will do it for us. But decreaseing nlink is
necessary. And I didn't actually clear out any memory in this part. For
removing dentry, I just set it to be inactive. For inode and datablock, I
just set their bitmap to be 0 to indicate that they can be overwritten.
Part11:
Hard link only add a dentry rather than a inode, so it is different from
creating file. But we can still learn from unlink(), we are just doing the
opposite thing here. In this part, I encounter a big bug because I directly
casted the buffer head rather than its b_data, but it works well finally.

Part12:
The only things need to notice are the mode of inode is S_IFLNK, and free
inode->ilink in free_inode(). All other things are similar to create a file.
