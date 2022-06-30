all: main.cpp parser.o fat32.h
	g++ -O3 --std=c++17 *.o main.cpp -o hw3
parser: parser.h parser.c
	g++ -O3 -c parser.c -o parser.o
image:
	dd if=/dev/zero of=example.img bs=1024 count=102400
	mkfs.vfat -F 32 -S 512 -s 2 example.img
mount:
	fusefat -o rw+ -o umask=770 example.img fs-root
unmount:
	fusermount -u fs-root
clean:
	fusermount -u fs-root
	rm -rf example.img hw3
info:
	fsck.vfat -vn example.img