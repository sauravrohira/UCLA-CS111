#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "ext2_fs.h"

#define BASE_OFFSET 1024

//Globals:
ssize_t file_system_fd;
struct ext2_super_block superblock;
struct ext2_group_desc* groups;
int num_groups;
__u32 block_size;

void superblock_summary()
{
  pread(file_system_fd, &superblock, sizeof(struct ext2_super_block), BASE_OFFSET);
  block_size = EXT2_MIN_BLOCK_SIZE << superblock.s_log_block_size;

  fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n", superblock.s_blocks_count, superblock.s_inodes_count, block_size, superblock.s_inode_size, superblock.s_blocks_per_group, superblock.s_inodes_per_group, superblock.s_first_ino);
}

void free_block_entries_summary(off_t offset, int bitmap_increment, int num_blocks)
{
    off_t block_offset = offset + bitmap_increment*block_size;
    long long block_bitmap;
    pread(file_system_fd, (void*) &block_bitmap, 1 + num_blocks/8, block_offset);
    int bit_num = 1;
    long long bit_value = 1;
    while(bit_num <= num_blocks)
    {
        if((block_bitmap & bit_value) == 0)
            fprintf(stdout, "BFREE,%d\n", bit_num);
        bit_num++;
        bit_value *= 2;
    }
}

void free_inode_entries_summary(off_t offset, int bitmap_increment, int num_inodes)
{
    off_t inode_map_offset = offset + bitmap_increment*block_size;
    long long inode_bitmap;
    pread(file_system_fd, (void*) &inode_bitmap, 1 + num_inodes/8, inode_map_offset);
    int bit_num = 1;
    long long bit_value = 1;
    while(bit_num <= num_inodes)
    {
        if((inode_bitmap & bit_value) == 0)
            fprintf(stdout, "IFREE,%d\n", bit_num);
        bit_num++;
        bit_value *= 2;
    }
}

void group_summary()
{
    num_groups = (superblock.s_inodes_count/superblock.s_inodes_per_group);
    groups = (struct ext2_group_desc*)malloc(num_groups*sizeof(struct ext2_group_desc));
    off_t offset = BASE_OFFSET + sizeof(struct ext2_super_block);
    
    for(int i = 0; i < num_groups; i++)
    {
        pread(file_system_fd, (void*) groups + i, sizeof(struct ext2_group_desc), offset);
        int num_blocks = superblock.s_blocks_count/num_groups;
        int num_inodes = superblock.s_inodes_count/num_groups;
        fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n", 
        i, 
        num_blocks, 
        num_inodes, 
        groups[i].bg_free_blocks_count,
        groups[i].bg_free_inodes_count,
        groups[i].bg_block_bitmap,
        groups[i].bg_inode_bitmap,
        groups[i].bg_inode_bitmap + 1
        );
        free_block_entries_summary(offset, (groups[i].bg_block_bitmap - 2), num_blocks);
        free_inode_entries_summary(offset, (groups[i].bg_inode_bitmap - 2), num_inodes);
        offset += num_blocks*block_size;
    }    
}


void dir_entries_summary(int block_id, int parent_inode)
{
  struct ext2_dir_entry dir;
  off_t offset = BASE_OFFSET + (block_id-1)*block_size;
  int counter = 0;
  while(counter < block_size)
    {
      pread(file_system_fd, &dir, sizeof(struct ext2_dir_entry), offset + counter);
      fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n", parent_inode, counter, dir.inode,dir.rec_len,dir.name_len,dir.name);
      counter += dir.rec_len;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Error: incorrect arguments, usage ./lab3a fs_image\n");
        exit(1);
    }

    const char * file_system = argv[1];
    file_system_fd = open(file_system, O_RDONLY);
    if (file_system_fd < 0)
    {
        fprintf(stderr, "Error: could not open file system image!\n");
        exit(2);
    }

    superblock_summary();
    group_summary();
}
