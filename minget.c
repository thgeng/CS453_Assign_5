/* 
 *  minget.c
 *
 * Copy a regular file from a MINIX V3 filesystem image to stdout or 
 * destination file.
 *  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minfs.h"

static void usage(const char *program_name)
{
    fprintf(stderr,
            "usage: %s [-v] [-p part [-s subpart]] imagefile srcpath "
            "[dstpath]\n",
            program_name);
}

/*
 *  * Print selected inode fields for verbose output.
 *   */
static void print_verbose_inode(uint32_t inode_number,
                                const Inode *inode)
{
    int zone_index;

    fprintf(stderr, "Inode %u:\n", inode_number);
    fprintf(stderr, "  mode       = 0%o\n", inode->mode);
    fprintf(stderr, "  links      = %u\n", inode->links);
    fprintf(stderr, "  uid        = %u\n", inode->uid);
    fprintf(stderr, "  gid        = %u\n", inode->gid);
    fprintf(stderr, "  size       = %u\n", inode->size);
    fprintf(stderr, "  indirect   = %u\n", inode->indirect);
    fprintf(stderr, "  d_indirect = %u\n", inode->two_indirect);

    for (zone_index = 0; zone_index < DIRECT_ZONES; zone_index++) {
        fprintf(stderr, "  zone[%d]    = %u\n",
                zone_index, inode->zone[zone_index]);
    }
}

static void print_verbose_superblock(long fs_offset,
                                     const Superblock *superblock)
{
    fprintf(stderr, "Filesystem offset = %ld bytes\n", fs_offset);
    printf(stderr, "Superblock:\n");
    fprintf(stderr, "  ninodes    = %u\n", superblock->ninodes);
    fprintf(stderr, "  i_blocks   = %d\n", superblock->i_blocks);
    fprintf(stderr, "  z_blocks   = %d\n", superblock->z_blocks);
    fprintf(stderr, "  firstdata  = %u\n", superblock->firstdata);
    fprintf(stderr, "  log_zone   = %d\n", superblock->log_zone_size);
    fprintf(stderr, "  zones      = %u\n", superblock->zones);
    fprintf(stderr, "  magic      = 0x%x\n", superblock->magic);
    fprintf(stderr, "  blocksize  = %u\n", superblock->blocksize);
    fprintf(stderr, "  subversion = %u\n", superblock->subversion);
}

/*
 *  * Parse command-line options for minget 
 *   * Positional arguments: imagefile srcpath [dstpath]
 *    */
static int parse_arguments(int argc, char *argv[],
                           int *verbose,
                           int *has_partition,
                           int *partition_number,
                           int *has_subpartition,
                           int *subpartition_number,
                           const char **image_path,
                           const char **src_path,
                           const char **dst_path)
{
    int arg_index;

    *verbose = 0;
    *has_partition = 0;
    *partition_number = 0;
    *has_subpartition = 0;
    *subpartition_number = 0;
    *image_path = NULL;
    *src_path = NULL;
    *dst_path = NULL;

    arg_index = 1;

    while (arg_index < argc && argv[arg_index][0] == '-') {
        if (strcmp(argv[arg_index], "-v") == 0) {
            *verbose = 1;
            arg_index++;
        } else if (strcmp(argv[arg_index], "-p") == 0) {
            arg_index++;
            if (arg_index >= argc) {
                fprintf(stderr, "Missing argument for -p\n");
                return -1;
            }
            *has_partition = 1;
            *partition_number = atoi(argv[arg_index]);
            arg_index++;
        } else if (strcmp(argv[arg_index], "-s") == 0) {
            arg_index++;
            if (arg_index >= argc) {
                fprintf(stderr, "Missing argument for -s\n");
                return -1;
            }
            *has_subpartition = 1;
            *subpartition_number = atoi(argv[arg_index]);
            arg_index++;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[arg_index]);
            return -1;
        }
    }

    if (*has_subpartition && !*has_partition) {
        fprintf(stderr, "-s requires -p\n");
        return -1;
    }

    if (arg_index >= argc) {
        fprintf(stderr, "Missing imagefile\n");
        return -1;
    }
    *image_path = argv[arg_index];
    arg_index++;

    if (arg_index >= argc) {
        fprintf(stderr, "Missing srcpath\n");
        return -1;
    }
    *src_path = argv[arg_index];
    arg_index++;

    if (arg_index < argc) {
        *dst_path = argv[arg_index];
        arg_index++;
    }

    if (arg_index != argc) {
        fprintf(stderr, "Too many arguments\n");
        return -1;
    }

    return 0;
}

FILE *image_fp = NULL;
FILE *out_fp = NULL;
char *buffer = NULL;

void cleanup(void);

int main(int argc, char *argv[])
{
    /*FILE *image_fp = NULL;
    FILE *out_fp = NULL;*/
    Superblock superblock;
    Inode target_inode;
    long fs_offset;
    uint32_t target_inode_number;
    const char *image_path;
    const char *src_path;
    const char *dst_path;
    int verbose;
    int has_partition;
    int partition_number;
    int has_subpartition;
    int subpartition_number;
    /*static char *buffer = NULL;*/

    if (parse_arguments(argc, argv,
                        &verbose,
                        &has_partition,
                        &partition_number,
                        &has_subpartition,
                        &subpartition_number,
                        &image_path,
                        &src_path,
                        &dst_path) != 0) {
        usage(argv[0]);
        return 1;
    }

    image_fp = fopen(image_path, "rb");
    if (image_fp == NULL) {
        perror("fopen image");
        return 1;
    }

    if (resolve_filesystem_offset(image_fp, has_partition,
                                  partition_number,
                                  has_subpartition,
                                  subpartition_number,
                                  &fs_offset, verbose) != 0) {
        cleanup();
    }

    if (read_superblock(image_fp, fs_offset, &superblock) != 0) {
        cleanup();
    }

    if (verbose) {
        print_verbose_superblock(fs_offset, &superblock);
    }

    if (resolve_path(image_fp, fs_offset, &superblock,
                     src_path, &target_inode_number,
                     &target_inode) != 0) {
        fprintf(stderr, "Path not found: %s\n", src_path);
        cleanup();
    }

    if (verbose) {
        print_verbose_inode(target_inode_number, &target_inode);
    }

    if (!is_regular_mode(target_inode.mode)) {
        fprintf(stderr, "%s is not a regular file\n", src_path);
        cleanup();
    }

    /* Open output destination (stdout if dstpath omitted) */
    if (dst_path != NULL) {
        out_fp = fopen(dst_path, "wb");
        if (out_fp == NULL) {
            perror("fopen output");
            cleanup();
        }
    } else {
        out_fp = stdout;
    }

    /* Allocate block buffer (size comes from superblock) */
    buffer = malloc(superblock.blocksize);
    if (buffer == NULL) {
        fprintf(stderr, "malloc failed\n");
        cleanup();
    }

    /* Copy file block-by-block (handles holes via read_file_block) */
    uint32_t remaining = target_inode.size;
    uint32_t block_index = 0;
    while (remaining > 0) {
        uint32_t bytes_to_read = (remaining > superblock.blocksize) ?
                                 superblock.blocksize : remaining;

        if (read_file_block(image_fp, fs_offset, &superblock,
                            &target_inode, block_index, buffer) != 0) {
            fprintf(stderr, "Failed to read file block\n");
            cleanup();
        }

        if (fwrite(buffer, 1, bytes_to_read, out_fp) != bytes_to_read) {
            fprintf(stderr, "Failed to write to output\n");
            cleanup();
        }

        remaining -= bytes_to_read;
        block_index++;
    }

    /* Success */
    if (out_fp != stdout) {
        fclose(out_fp);
        out_fp = NULL;
    }
    free(buffer);
    fclose(image_fp);
    return 0;
}

void cleanup(void){
    if (buffer) free(buffer);
    if (out_fp && out_fp != stdout) fclose(out_fp);
    if (image_fp) fclose(image_fp);
    exit(1);
}
