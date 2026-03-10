#include <stdio.h>
#include "minfs.h"

int main(int argc, char *argv[])
{
    FILE *fp;
    Superblock sb;
    Inode inode;

    if (argc != 2) {
        fprintf(stderr, "usage: %s imagefile\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    if (read_superblock(fp, 0, &sb) != 0) {
        fprintf(stderr, "Failed to read superblock\n");
        fclose(fp);
        return 1;
    }

    if (read_inode(fp, 0, &sb, 1, &inode) != 0) {
        fprintf(stderr, "Failed to read root inode\n");
        fclose(fp);
        return 1;
    }

    printf("Root inode read successfully\n");
    printf("mode: 0%o\n", inode.mode);
    printf("size: %u\n", inode.size);
    printf("zone[0]: %u\n", inode.zone[0]);
    printf("zone[1]: %u\n", inode.zone[1]);

    fclose(fp);
    return 0;
}

