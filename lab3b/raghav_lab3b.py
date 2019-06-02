#!/usr/bin/python

import csv


class superblock:
    def __init__(self, row):
        self.num_blocks = row[1]
        self.num_inodes = row[2]
        self.block_size = row[3]
        self.inode_size = row[4]
        self.blocks_per_group = row[5]
        self.inodes_per_group = row[6]
        self.non_res_inode = row[7]

class inode:
    def __init__(self, row):
        self.inode_num = row[1]
        self.file_type = row[2]
        self.mode = row[3]
        self.owner = row[4]
        self.group = row[5]
        self.link_count = row[6]
        self.file_size = row[10]
        self.num_blocks = row[11]
        self.block_address = []
        for i in range(12,27):
            self.block_address.append(row[i])


try:
    csv_file = open(sys.arg[1])
    csv_reader = csv.reader(csv_file)

except:
    #error

