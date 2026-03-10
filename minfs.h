#ifndef MINFS_H
#define MINFS_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Disk and MINIX filesystem constants */
#define SECTOR_SIZE              512
#define PARTITION_TABLE_OFFSET   0x1BE
#define PARTITION_MAGIC1         0x55
#define PARTITION_MAGIC2         0xAA
#define MINIX_PARTITION_TYPE     0x81
#define MINIX_MAGIC              0x4D5A

#define INODE_SIZE               64
#define DIR_ENTRY_SIZE           64
#define DIRECT_ZONES             7
#define ROOT_INODE               1
#define PARTITION_COUNT          4

/* Mode bit masks */
#define MODE_TYPE_MASK           0170000
#define MODE_REGULAR             0100000
#define MODE_DIRECTORY           0040000

typedef struct __attribute__((packed)) PartitionEntry {
    uint8_t  bootind;
    uint8_t  start_head;
    uint8_t  start_sec;
    uint8_t  start_cyl;
    uint8_t  type;
    uint8_t  end_head;
    uint8_t  end_sec;
    uint8_t  end_cyl;
    uint32_t lFirst;
    uint32_t size;
} PartitionEntry;

typedef struct __attribute__((packed)) Superblock {
    uint32_t ninodes;
    uint16_t pad1;
    int16_t  i_blocks;
    int16_t  z_blocks;
    uint16_t firstdata;
    int16_t  log_zone_size;
    int16_t  pad2;
    uint32_t max_file;
    uint32_t zones;
    int16_t  magic;
    int16_t  pad3;
    uint16_t blocksize;
    uint8_t  subversion;
} Superblock;

typedef struct __attribute__((packed)) Inode {
    uint16_t mode;
    uint16_t links;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t  atime;
    int32_t  mtime;
    int32_t  ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
} Inode;

typedef struct __attribute__((packed)) DirEntry {
    uint32_t inode;
    char name[60];
} DirEntry;

/* Low-level image helpers */
int read_bytes(FILE *image_fp, long offset, void *buffer, size_t size);

/* Partition helpers */
int read_partition_table(FILE *image_fp, long table_offset,
                         PartitionEntry table[PARTITION_COUNT]);

int resolve_filesystem_offset(FILE *image_fp, int has_partition,
                              int partition_number, int has_subpartition,
                              int subpartition_number, long *fs_offset,
                              int verbose);

/* Filesystem structure helpers */
int read_superblock(FILE *image_fp, long fs_offset,
                    Superblock *superblock);

int read_inode(FILE *image_fp, long fs_offset,
               const Superblock *superblock,
               uint32_t inode_number, Inode *inode);

/* File block helpers */
uint32_t get_file_zone(FILE *image_fp, long fs_offset,
                       const Superblock *superblock,
                       const Inode *inode,
                       uint32_t file_block_index);

int read_file_block(FILE *image_fp, long fs_offset,
                    const Superblock *superblock,
                    const Inode *inode,
                    uint32_t file_block_index, void *buffer);

/* Directory helpers */
int find_in_directory(FILE *image_fp, long fs_offset,
                      const Superblock *superblock,
                      const Inode *directory_inode,
                      const char *name,
                      uint32_t *inode_number_out);

int resolve_path(FILE *image_fp, long fs_offset,
                 const Superblock *superblock,
                 const char *path,
                 uint32_t *inode_number_out,
                 Inode *result_inode);

int list_directory(FILE *image_fp, long fs_offset,
                   const Superblock *superblock,
                   const Inode *directory_inode,
                   const char *display_path);

/* Formatting helpers */
int is_directory_mode(uint16_t mode);
int is_regular_mode(uint16_t mode);
void mode_to_string(uint16_t mode, char out[11]);
void print_inode_summary(const Inode *inode, const char *name_or_path);

#endif
