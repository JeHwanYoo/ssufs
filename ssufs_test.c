#include "ssufs-ops.h"

int main()
{
	/** test 1 **/
  char str[] = "!-------32 Bytes of Data-------!!-------32 Bytes of Data-------!!--over--!";
  char str2[] = "!-------32 Bytes of F2TX-------!!-------32 Bytes of Data-------!!--over2--!";
	char str3[256];
	ssufs_formatDisk();
	ssufs_create("f1.txt");
	ssufs_create("f2.txt");
	int fd1 = ssufs_open("f1.txt");
	int fd2 = ssufs_open("f2.txt");
  printf("Write Data: %d\n", ssufs_write(fd1, str, BLOCKSIZE + BLOCKSIZE));
  printf("Seek: %d\n", ssufs_lseek(fd1, -2));
  printf("Read Data: %d\n", ssufs_read(fd1, str3, 70));
  printf("Read Data: %s\n", str3);
	ssufs_dump();
	/*
    char str[] = "!-------32 Bytes of Data-------!!-------32 Bytes of Data-------!";
    ssufs_formatDisk();

    ssufs_create("f1.txt");
    int fd1 = ssufs_open("f1.txt");

    printf("Write Data: %d\n", ssufs_write(fd1, str, BLOCKSIZE));
    printf("Seek: %d\n", ssufs_lseek(fd1, 0));
    ssufs_dump();
    ssufs_delete("f1.txt");
    ssufs_dump();
		*/
}
