#include <stdio.h>
#include "minfs.h"

int main(int argc, char *argv[])
{
    FILE *fp;
    Superblock sb;
    Inode root;

    if (argc != 2) {
        fprintf(stderr, "usage: %s image\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    if (read_superblock(fp, 0, &sb) != 0) {
        fprintf(stderr, "Failed to read superblock\n");
        fclose(fp);
        return 1;
    }

    if (read_inode(fp, 0, &sb, 1, &root) != 0) {
        fprintf(stderr, "Failed to read root inode\n");
        fclose(fp);
        return 1;
    }

    list_directory(fp, 0, &sb, &root);

    fclose(fp);
    return 0;
}
