#include <stdio.h>
#include "minfs.h"

int main(void)
{
    printf("PartitionEntry: %zu\n", sizeof(PartitionEntry));
    printf("Superblock: %zu\n", sizeof(Superblock));
    printf("Inode: %zu\n", sizeof(Inode));
    printf("DirEntry: %zu\n", sizeof(DirEntry));
    return 0;
}
