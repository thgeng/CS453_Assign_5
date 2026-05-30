/*
 * minls.c
 *
 * List files and directories stored inside a MINIX V3 filesystem image.
 * systems used in the course environment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minfs.h"

static void usage(const char *program_name)
{
    fprintf(stderr,
            "usage: %s [-v] [-p part [-s subpart]] imagefile [path]\n",
            program_name);
}

/*
 * Print selected inode fields for verbose output.
 * All verbose information goes to stderr, as required by the assignment.
 */
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

/*Strips Leading '/' for display formatting*/
static const char *get_file_display_name(const char *path)
{
    if ((path == NULL) || (path[0] == '\0')) return ".";

    /*Skip Leading '/'*/
    while (*path == '/') path++;

    /*If path is '/', root*/
    return (*path == '\0') ? "." : path;
}

/*
 *  * Parse command-line options without getopt.
 *
 * Supported options:
 *   -v
 *   -p part
 *   -s subpart
 *
 * On success, fills output parameters and returns 0.
 * On failure, returns -1.
 */
static int parse_arguments(int argc, char *argv[],
                           int *verbose,
                           int *has_partition,
                           int *partition_number,
                           int *has_subpartition,
                           int *subpartition_number,
                           const char **image_path,
                           const char **lookup_path)
{
    int arg_index;

    *verbose = 0;
    *has_partition = 0;
    *partition_number = 0;
    *has_subpartition = 0;
    *subpartition_number = 0;
    *image_path = NULL;
    *lookup_path = "/";

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

    if (arg_index < argc) {
        *lookup_path = argv[arg_index];
        arg_index++;
    }

    if (arg_index != argc) {
        fprintf(stderr, "Too many arguments\n");
        return -1;
    }

    return 0;
}

static void print_verbose_superblock(long fs_offset,
                                     const Superblock *superblock)
{
    fprintf(stderr, "Filesystem offset = %ld bytes\n", fs_offset);
    fprintf(stderr, "Superblock:\n");
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

int main(int argc, char *argv[])
{
    FILE *image_fp;
    Superblock superblock;
    Inode target_inode;
    long fs_offset;
    uint32_t target_inode_number;
    const char *image_path;
    const char *lookup_path;
    int verbose;
    int has_partition;
    int partition_number;
    int has_subpartition;
    int subpartition_number;

    if (parse_arguments(argc, argv,
                        &verbose,
                        &has_partition,
                        &partition_number,
                        &has_subpartition,
                        &subpartition_number,
                        &image_path,
                        &lookup_path) != 0) {
        usage(argv[0]);
        return 1;
    }

    image_fp = fopen(image_path, "rb");
    if (image_fp == NULL) {
        perror("fopen");
        return 1;
    }

    if (resolve_filesystem_offset(image_fp, has_partition,
                                  partition_number,
                                  has_subpartition,
                                  subpartition_number,
                                  &fs_offset, verbose) != 0) {
        fclose(image_fp);
        return 1;
    }

    if (read_superblock(image_fp, fs_offset, &superblock) != 0) {
        fclose(image_fp);
        return 1;
    }

    if (verbose) {
        print_verbose_superblock(fs_offset, &superblock);
    }

    if (resolve_path(image_fp, fs_offset, &superblock,
                     lookup_path, &target_inode_number,
                     &target_inode) != 0) {
        fprintf(stderr, "Path not found: %s\n", lookup_path);
        fclose(image_fp);
        return 1;
    }

    if (verbose) {
        print_verbose_inode(target_inode_number, &target_inode);
    }

    if (is_directory_mode(target_inode.mode)) {
        if (list_directory(image_fp, fs_offset, &superblock,
                           &target_inode, lookup_path) != 0) {
            fclose(image_fp);
            return 1;
        }
    } else if (is_regular_mode(target_inode.mode)) {
        print_inode_summary(&target_inode, get_file_display_name(lookup_path));
    } else {
        fprintf(stderr, "Unsupported file type: %s\n", lookup_path);
        fclose(image_fp);
        return 1;
    }

    fclose(image_fp);
    return 0;
}
