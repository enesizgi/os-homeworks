image:
	dd if=/dev/zero of=example.img bs=1024 count=102400
	mkfs.vfat -F 32 -S 512 -s 2 example.img
mount:
	fusefat -o rw+ -o umask=770 example.img fs-root
unmount:
	fusermount -u fs-root
clean:
	fusermount -u fs-root
	rm -rf example.img