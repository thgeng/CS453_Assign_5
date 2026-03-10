#include <stdio.h>
#include "minfs.h"

int main(int argc, char *argv[])
{
    FILE *fp;
    Superblock sb;

    if (argc != 2) {
        fprintf(stderr, "usage: %s imagefile\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    if (read_superblock(fp, 0, &sb) != 0) {
        fprintf(stderr, "Failed to read superblock\n");
        return 1;
    }

    printf("Superblock read successfully\n");
    printf("ninodes: %u\n", sb.ninodes);
    printf("blocksize: %u\n", sb.blocksize);
    printf("zones: %u\n", sb.zones);
    printf("firstdata: %u\n", sb.firstdata);

    fclose(fp);
    return 0;
}
