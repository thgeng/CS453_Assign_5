#include "minfs.h"

/* Convert Azone number to starting block number. 
 * Zone 0 is special (never valid data zone). */
static uint32_t zone_to_block(const Superblock *superblock, uint32_t zone)
{
    if (zone == 0) return 0;
    return zone << superblock->log_zone_size;
}

static void print_partition_entry(const PartitionEntry *entry, int index)
{
    fprintf(stderr,
            "Partition %d: type=0x%02x start=%u size=%u\n",
            index, entry->type, entry->lFirst, entry->size);
}

static void print_superblock_verbose(const Superblock *superblock)
{
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

int read_bytes(FILE *image_fp, long offset, void *buffer, size_t size)
{
    if (fseek(image_fp, offset, SEEK_SET) != 0) {
        return -1;
    }

    if (fread(buffer, 1, size, image_fp) != size) {
        return -1;
    }

    return 0;
}

int read_partition_table(FILE *image_fp, long table_offset,
                         PartitionEntry table[PARTITION_COUNT])
{
    unsigned char signature[2];

    if (read_bytes(image_fp, table_offset + PARTITION_TABLE_OFFSET,
                   table,
                   PARTITION_COUNT * sizeof(PartitionEntry)) != 0) {
        return -1;
    }

    if (read_bytes(image_fp, table_offset + 510, signature, 2) != 0) {
        return -1;
    }

    if (signature[0] != PARTITION_MAGIC1 ||
        signature[1] != PARTITION_MAGIC2) {
        return -1;
    }

    return 0;
}

int resolve_filesystem_offset(FILE *image_fp, int has_partition,
                              int partition_number, int has_subpartition,
                              int subpartition_number, long *fs_offset,
                              int verbose)
{
    PartitionEntry table[PARTITION_COUNT];
    PartitionEntry chosen;

    *fs_offset = 0;

    if (!has_partition) {
        return 0;
    }

    if (partition_number < 0 || partition_number >= PARTITION_COUNT) {
        fprintf(stderr, "Invalid partition number: %d\n",
                partition_number);
        return -1;
    }

    if (read_partition_table(image_fp, 0, table) != 0) {
        fprintf(stderr, "Invalid partition table in image\n");
        return -1;
    }

    if (verbose) {
        int i;

        for (i = 0; i < PARTITION_COUNT; i++) {
            print_partition_entry(&table[i], i);
        }
    }

    chosen = table[partition_number];
    if (chosen.type != MINIX_PARTITION_TYPE) {
        fprintf(stderr, "Requested partition is not a MINIX partition\n");
        return -1;
    }

    *fs_offset = (long) chosen.lFirst * SECTOR_SIZE;

    if (!has_subpartition) {
        return 0;
    }

    if (subpartition_number < 0 || subpartition_number >= PARTITION_COUNT) {
        fprintf(stderr, "Invalid subpartition number: %d\n",
                subpartition_number);
        return -1;
    }

    if (read_partition_table(image_fp, *fs_offset, table) != 0) {
        fprintf(stderr, "Invalid subpartition table\n");
        return -1;
    }

    if (verbose) {
        int i;

        for (i = 0; i < PARTITION_COUNT; i++) {
            fprintf(stderr, "Sub");
            print_partition_entry(&table[i], i);
        }
    }

    chosen = table[subpartition_number];
    if (chosen.type != MINIX_PARTITION_TYPE) {
        fprintf(stderr,
                "Requested subpartition is not a MINIX partition\n");
        return -1;
    }

    *fs_offset = (long) chosen.lFirst * SECTOR_SIZE;

    return 0;
}

int read_superblock(FILE *image_fp, long fs_offset,
                    Superblock *superblock)
{
    long superblock_offset;

    superblock_offset = fs_offset + 1024;

    if (read_bytes(image_fp, superblock_offset,
                   superblock, sizeof(Superblock)) != 0) {
        return -1;
    }

    if (superblock->magic != MINIX_MAGIC) {
        fprintf(stderr, "Invalid superblock magic: 0x%x\n",
                superblock->magic);
        return -1;
    }

    return 0;
}

int read_inode(FILE *image_fp, long fs_offset,
               const Superblock *superblock,
               uint32_t inode_number, Inode *inode)
{
    long inode_table_block;
    long inode_table_offset;
    long inode_offset;

    if (inode_number == 0 || inode_number > superblock->ninodes) {
        return -1;
    }

    inode_table_block = 2 + superblock->i_blocks +
                        superblock->z_blocks;
    inode_table_offset = fs_offset +
                         inode_table_block * superblock->blocksize;
    inode_offset = inode_table_offset +
                   (inode_number - 1) * INODE_SIZE;

    return read_bytes(image_fp, inode_offset, inode, sizeof(Inode));
}

/*
 * get_file_zone: Return the zone number for the zone containing the given
 * file_block_index (0-based block number in the file's data).
 * Returns 0 for holes (unallocated zones) or invalid/out-of-range.
 * Supports direct, single-indirect, and double-indirect zones.
 * This enables correct handling of sparse files (holes) at all levels.
 */
uint32_t get_file_zone(FILE *image_fp, long fs_offset,
                       const Superblock *superblock,
                       const Inode *inode,
                       uint32_t file_block_index)
{
    uint32_t entries_per_indirect;
    uint32_t logic_zone_idx;
    uint32_t zone_number;

    entries_per_indirect = superblock->blocksize / sizeof(uint32_t);
    logic_zone_idx = file_block_index >> superblock->log_zone_size;

    /* Direct zones */
    if (logic_zone_idx < DIRECT_ZONES) {
        return inode->zone[logic_zone_idx];
    }
    logic_zone_idx -= DIRECT_ZONES;

    /* Single indirect */
    if (logic_zone_idx < entries_per_indirect) {
        uint32_t indirect_zone;
        uint32_t indirect_block;
        long indirect_offset;

        if (inode->indirect == 0) {
            return 0;  /* hole or unallocated indirect */
        }

        indirect_zone = inode->indirect;
        indirect_block = zone_to_block(superblock, indirect_zone);
        indirect_offset = fs_offset+(long)indirect_block*superblock->blocksize
                          + logic_zone_idx * sizeof(uint32_t);

        if (read_bytes(image_fp, 
                indirect_offset, 
                &zone_number, 
                sizeof(uint32_t)) != 0) {
            return 0;
        }
        return zone_number;  /* may be 0 for hole in this indirect entry */
    }
    logic_zone_idx -= entries_per_indirect;

    /* Double indirect */
    if (logic_zone_idx<(uint64_t)entries_per_indirect * entries_per_indirect) {
        uint32_t d1, d2;  /* indices in first and second level */
        uint32_t d_indirect_zone;
        uint32_t d_indirect_block;
        long d_offset;
        uint32_t single_indirect_zone;  /* zone# of the single-indirect zone */
        uint32_t single_indirect_block;
        long s_offset;

        d1 = logic_zone_idx / entries_per_indirect;
        d2 = logic_zone_idx % entries_per_indirect;

        if (inode->two_indirect == 0) {
            return 0;  /* no double indirect allocated->hole for this range */
        }

        d_indirect_zone = inode->two_indirect;
        d_indirect_block = zone_to_block(superblock, d_indirect_zone);
        d_offset = fs_offset + (long)d_indirect_block * superblock->blocksize
                   + d1 * sizeof(uint32_t);

        if (read_bytes(image_fp, 
                d_offset, 
                &single_indirect_zone, 
                sizeof(uint32_t)) != 0) {
            return 0;
        }
        if (single_indirect_zone == 0) {
            return 0;  /* hole at first level of double-indirect */
        }

        /* Now lookup in the single-indirect zone's first block */
        single_indirect_block=zone_to_block(superblock, single_indirect_zone);
        s_offset = fs_offset+(long)single_indirect_block*superblock->blocksize
                   + d2 * sizeof(uint32_t);

        if (read_bytes(image_fp, 
                s_offset, 
                &zone_number, 
                sizeof(uint32_t)) != 0) {
            return 0;
        }
        return zone_number;  /* may be 0 -> hole */
    }

    /* Beyond supported range (triple indirect not in Minix V3) */
    return 0;
}

int read_file_block(FILE *image_fp, long fs_offset,
                    const Superblock *superblock,
                    const Inode *inode,
                    uint32_t file_block_index, void *buffer)
{
    uint32_t zone_number = get_file_zone(image_fp, fs_offset, 
                                superblock, inode, file_block_index);

    if (zone_number == 0) {
        /* Hole:entire zone (thus this block) is zeros. Per assignment spec. */
        memset(buffer, 0, superblock->blocksize);
        return 0;
    }

    uint32_t block_number = zone_to_block(superblock, zone_number);

    if (superblock->log_zone_size > 0) {
        block_number+=file_block_index&((1U << superblock->log_zone_size) - 1);
    }

    long block_offset = fs_offset + (long)block_number * superblock->blocksize;

    return read_bytes(image_fp, block_offset, buffer, superblock->blocksize);
}

int is_directory_mode(uint16_t mode)
{
    return (mode & MODE_TYPE_MASK) == MODE_DIRECTORY;
}

int is_regular_mode(uint16_t mode)
{
    return (mode & MODE_TYPE_MASK) == MODE_REGULAR;
}

void mode_to_string(uint16_t mode, char out[11])
{
    out[0] = is_directory_mode(mode) ? 'd' : '-';
    out[1] = (mode & 0400) ? 'r' : '-';
    out[2] = (mode & 0200) ? 'w' : '-';
    out[3] = (mode & 0100) ? 'x' : '-';
    out[4] = (mode & 0040) ? 'r' : '-';
    out[5] = (mode & 0020) ? 'w' : '-';
    out[6] = (mode & 0010) ? 'x' : '-';
    out[7] = (mode & 0004) ? 'r' : '-';
    out[8] = (mode & 0002) ? 'w' : '-';
    out[9] = (mode & 0001) ? 'x' : '-';
    out[10] = '\0';
}

void print_inode_summary(const Inode *inode, const char *name_or_path)
{
    char mode_string[11];

    mode_to_string(inode->mode, mode_string);
    printf("%s %9u %s\n", mode_string, inode->size, name_or_path);
}

int find_in_directory(FILE *image_fp, long fs_offset,
                      const Superblock *superblock,
                      const Inode *directory_inode,
                      const char *name,
                      uint32_t *inode_number_out)
{
    char *block_buffer;
    DirEntry *entries;
    uint32_t total_entries;
    uint32_t entries_per_block;
    uint32_t blocks_needed;
    uint32_t block_index;
    uint32_t entry_index;
    uint32_t entries_seen;

    block_buffer = malloc(superblock->blocksize);
    if (block_buffer == NULL) {
        return -1;
    }

    total_entries = directory_inode->size / DIR_ENTRY_SIZE;
    entries_per_block = superblock->blocksize / DIR_ENTRY_SIZE;
    blocks_needed = (directory_inode->size +
                     superblock->blocksize - 1) /
                    superblock->blocksize;
    entries_seen = 0;

    for (block_index = 0;
         block_index < blocks_needed;
         block_index++) {

        if (read_file_block(image_fp, fs_offset, superblock,
                            directory_inode, block_index,
                            block_buffer) != 0) {
            free(block_buffer);
            return -1;
        }

        entries = (DirEntry *) block_buffer;

        for (entry_index = 0;
             entry_index < entries_per_block &&
             entries_seen < total_entries;
             entry_index++, entries_seen++) {

            if (entries[entry_index].inode == 0) {
                continue;
            }

            char safe_name[61];
            memcpy(safe_name, entries[entry_index].name, 60);
            safe_name[60] = '\0';

            if (strcmp(safe_name, name) == 0) {
                *inode_number_out = entries[entry_index].inode;
                free(block_buffer);
                return 0;
            }
        }
    }

    free(block_buffer);
    return -1;
}

int resolve_path(FILE *image_fp, long fs_offset,
                 const Superblock *superblock,
                 const char *path,
                 uint32_t *inode_number_out,
                 Inode *result_inode)
{
    Inode current_inode;
    uint32_t current_inode_number;
    uint32_t next_inode_number;
    char path_copy[1024];
    char *component;

    if (strlen(path) >= sizeof(path_copy)) {
        return -1;
    }

    current_inode_number = ROOT_INODE;
    if (read_inode(image_fp, fs_offset, superblock,
                   current_inode_number, &current_inode) != 0) {
        return -1;
    }

    if (path[0] == '\0' || strcmp(path, "/") == 0) {
        *inode_number_out = current_inode_number;
        *result_inode = current_inode;
        return 0;
    }

    strcpy(path_copy, path);
    component = strtok(path_copy, "/");

    while (component != NULL) {
        if (!is_directory_mode(current_inode.mode)) {
            return -1;
        }

        if (find_in_directory(image_fp, fs_offset, superblock,
                              &current_inode, component,
                              &next_inode_number) != 0) {
            return -1;
        }

        if (read_inode(image_fp, fs_offset, superblock,
                       next_inode_number, &current_inode) != 0) {
            return -1;
        }

        current_inode_number = next_inode_number;
        component = strtok(NULL, "/");
    }

    *inode_number_out = current_inode_number;
    *result_inode = current_inode;
    return 0;
}

int list_directory(FILE *image_fp, long fs_offset,
                   const Superblock *superblock,
                   const Inode *directory_inode,
                   const char *display_path)
{
    char *block_buffer;
    DirEntry *entries;
    uint32_t total_entries;
    uint32_t entries_per_block;
    uint32_t blocks_needed;
    uint32_t block_index;
    uint32_t entry_index;
    uint32_t entries_seen;

    if (!is_directory_mode(directory_inode->mode)) {
        fprintf(stderr, "%s is not a directory\n", display_path);
        return -1;
    }

    block_buffer = malloc(superblock->blocksize);
    if (block_buffer == NULL) {
        fprintf(stderr, "malloc failed in list_directory\n");
        return -1;
    }

    //printf("%s:\n", display_path);
    char canonical_path[1024];
    const char *src = display_path;
    char *dst = canonical_path;
    *dst++ = '/';
    int last_was_slash = 1;
    while (*src) {
        if (*src == '/') {
            if (!last_was_slash) {
                *dst++ = '/';
                last_was_slash = 1;
            }
            src++;
        } else {
            *dst++ = *src++;
            last_was_slash = 0;
        }
    }
    if (dst > canonical_path + 1 && *(dst-1) == '/') {
        dst--;
    }
    *dst = '\0';
    printf("%s:\n", canonical_path);


    total_entries = directory_inode->size / DIR_ENTRY_SIZE;
    entries_per_block = superblock->blocksize / DIR_ENTRY_SIZE;
    blocks_needed = (directory_inode->size +
                     superblock->blocksize - 1) /
                    superblock->blocksize;
    entries_seen = 0;

    for (block_index = 0;
         block_index < blocks_needed;
         block_index++) {

        if (read_file_block(image_fp, fs_offset, superblock,
                            directory_inode, block_index,
                            block_buffer) != 0) {
            fprintf(stderr, "Failed to read directory block\n");
            free(block_buffer);
            return -1;
        }

        entries = (DirEntry *) block_buffer;

        for (entry_index = 0;
             entry_index < entries_per_block &&
             entries_seen < total_entries;
             entry_index++, entries_seen++) {

            Inode entry_inode;

            if (entries[entry_index].inode == 0) {
                continue;
            }

            char safe_name[61];
            memcpy(safe_name, entries[entry_index].name, 60);
            safe_name[60] = '\0';


            if (read_inode(image_fp, fs_offset, superblock,
                           entries[entry_index].inode,
                           &entry_inode) != 0) {
                free(block_buffer);
                return -1;
            }

            print_inode_summary(&entry_inode, safe_name);
        }
    }

    free(block_buffer);
    return 0;
}
