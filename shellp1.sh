umount /mnt/pantry &
losetup --detach /dev/loop0 &
rm -r /mnt/pantry &
rmmod pantry &
dd bs=4096 count=200 if=/dev/zero of=~/pantry_disk.img &
losetup --find --show ~/pantry_disk.img &
./format_disk_as_pantryfs /dev/loop0 &
mkdir /mnt/pantry &
insmod -f ref/pantry-x86.ko &
mount -t pantryfs /dev/loop0 /mnt/pantry &

