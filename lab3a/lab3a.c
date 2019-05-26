#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ext2_fs.h"

#define BASE_OFFSET 1024
#define EXT2_S_IFLNK 0xA000 //symbolic link
#define EXT2_S_IFREG 0x8000 //regular file
#define EXT2_S_IFDIR 0x4000 //directory

//Globals:
ssize_t file_system_fd;
struct ext2_super_block superblock;
struct ext2_group_desc* groups;
int num_groups;
__u32 block_size;
int logical_offset[3] = {12, 268, 65804};


void convert_time(__u32 old_time, char * new_time)
{
    time_t t = (time_t)old_time;
    struct tm ts;
    ts = *gmtime(&t);
    strftime(new_time, 20, "%m/%d/%y %H:%M:%S", &ts);
}

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

void dir_entries_summary(int block_id, int parent_inode)
{
    struct ext2_dir_entry dir;
    off_t offset = BASE_OFFSET + (block_id - 1) * block_size;
    __u32 counter = 0;
    while (counter < block_size)
    {
        pread(file_system_fd, &dir, sizeof(struct ext2_dir_entry), offset + counter);
        fprintf(stdout, "DIRENT,%d,%d,%d,%d,%d,'%s'\n", parent_inode, counter, dir.inode, dir.rec_len, dir.name_len, dir.name);
        counter += dir.rec_len;
    }
}

void ind_blocks_summary(int curr_level, int inode_num, int curr_block_num, int logical_offset)
{
    __u32 blockOffset = curr_block_num * block_size;
    __u32 blockValue;

    for (__u32 i = 0; i < block_size / 4; i++)
    {
        if (pread(file_system_fd, &blockValue, sizeof(blockValue), blockOffset + i * 4) < 0)
            fprintf(stdout, "Error!\n");

        if (blockValue != 0)
        {
            fprintf(stdout, "INDIRECT,%u,%u,%u,%u,%u\n", inode_num, curr_level, logical_offset, curr_block_num, blockValue);
            if(curr_level == 1)
                (logical_offset)++;
            else
                ind_blocks_summary(curr_level - 1, inode_num, blockValue, logical_offset);
        }
        else if(curr_level == 1)
            (logical_offset)++;
    }
}

void inode_summary(off_t offset, int inode_table_increment, int num_inodes)
{
    struct ext2_inode curr_inode;
    off_t inode_table_offset = offset + inode_table_increment * block_size;

    for (int i = 1; i <= num_inodes; i++)
    {
        pread(file_system_fd, &curr_inode, sizeof(struct ext2_inode), inode_table_offset);
        if(curr_inode.i_mode != 0 && curr_inode.i_links_count != 0)
        {
            int inode_num = i;
            char filetype = '?';
            __u16 mode = (curr_inode.i_mode >> 12) << 12;
            if(mode == EXT2_S_IFREG)
                filetype = 'f';
            if (mode == EXT2_S_IFLNK)
                filetype = 's';
            if (mode == EXT2_S_IFDIR)
                filetype = 'd';
            char ctime[20];
            char mtime[20];
            char atime[20];
            convert_time(curr_inode.i_ctime, ctime);
            convert_time(curr_inode.i_mtime, mtime);
            convert_time(curr_inode.i_atime, atime);

            fprintf(stdout,"INODE,%d,%c,%o,%d,%d,%d,%s,%s,%s,%d,%d",
            inode_num, 
            filetype, 
            (curr_inode.i_mode & 0xFFF), 
            curr_inode.i_uid, 
            curr_inode.i_gid, 
            curr_inode.i_links_count,
            ctime,
            mtime,
            atime,
            curr_inode.i_size,
            curr_inode.i_blocks
            );
            if( ! (filetype == 's' && curr_inode.i_blocks == 0))
                for(int i = 0; i < 15; i++)
                {
                    fprintf(stdout, ",%d", curr_inode.i_block[i]);
                }
            fprintf(stdout, "\n");

            if(filetype == 'd')
                dir_entries_summary(curr_inode.i_block[0],inode_num);
            
            for(int i = 1; i <= 3; i++)
            {
                if (curr_inode.i_block[11 + i] != 0)
                    ind_blocks_summary(i, inode_num, curr_inode.i_block[11 + i], logical_offset[i - 1]);
            }
        }
        inode_table_offset += sizeof(struct ext2_inode);
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
        inode_summary(offset, (groups[i].bg_inode_bitmap - 1), num_inodes);
        offset += num_blocks*block_size;
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
        exit(1);
    }

    superblock_summary();
    group_summary();

    exit(0);
}