#!/bin/python

import sys
import csv
import os

class Superblock:
    def __init__(self, row):
        self.num_blocks = int(row[1])
        self.num_inodes = int(row[2])
        self.block_size = int(row[3])
        self.inode_size = int(row[4])
        self.blocks_per_group = int(row[5])
        self.inodes_per_group = int(row[6])
        self.first_unreserved = int(row[7])

class Group:
    def __init__(self, row):
        self.group_num = int(row[1])
        self.num_blocks = int(row[2])
        self.num_inodes = int(row[3])
        self.num_free_blocks = int(row[4])
        self.num_free_inodes = int(row[5])


class Inode:
    def __init__(self, row):
        self.inode_num = int(row[1])
        self.file_type = row[2]
        self.mode = int(row[3])
        self.owner = int(row[4])
        self.group = int(row[5])
        self.link_count = int(row[6])
        self.file_size = int(row[10])
        self.num_blocks = int(row[11])
        self.blocks = []
        self.has_blocks = 0
        if self.file_type == "f" or self.file_type == "d" or (self.file_type == "s" and self.file_size > 60):
            self.has_blocks = 1
            for i in range(12, 27): 
                self.blocks.append(int(row[i]))
            
class Directory:
    def __init__(self, row):
        self.parent_inode = int(row[1])
        self.byte_offset = int(row[2])
        self.referenced_inode = int(row[3])
        self.entry_len = int(row[4])
        self.name_len = int(row[5])
        self.name = row[6]

class IndirectBlock:
    def __init__(self, row):
        self.parent_inode = int(row[1])
        self.level = int(row[2])
        self.logical_offset = int(row[3])
        self.block_num = int(row[4])
        self.referenced_block = int(row[5])

try:
    csv_file = open(sys.argv[1])
    csv_reader = csv.reader(csv_file)
except:
    sys.stderr.write("Error: could not read csv file!\n")
    sys.exit(1)

flaws = False
groupList = []
inodeList = []
dirList = []
indirectblockList = []
free_block_nums = []
free_inode_nums = []
unallocated_inode_nums=[]
allocated_inode_nums=[]
allocated_block_nums = []
duplicate_output_messages = []
offset_val = [0, 12, 268, 65804]
parent_to_child={}

def add_message(block_num, inode_num, indirection_num, offset_num):
    flaws = True
    indirection_str = ""
    if indirection_num == 1:
        indirection_str = "INDIRECT "
    if indirection_num == 2:
        indirection_str = "DOUBLE INDIRECT "
    if indirection_num == 3:
        indirection_str = "TRIPLE INDIRECT"
    output_str = "DUPLICATE " + indirection_str + "BLOCK " + str(block_num) + " IN INODE " + str(inode_num) + " AT OFFSET " + str(offset_num)
    duplicate_output_messages.append(output_str)

num_groups_seen = 0
dir_entry_status = 0

for row in csv_reader:
    if row[0] == "SUPERBLOCK":
        s_block = Superblock(row)
    if row[0] == "GROUP":
        groupList.append(Group(row))
        num_groups_seen = num_groups_seen + 1
    if row[0] == "INODE":
        inode = Inode(row)
        inodeList.append(inode)
        if(inode.has_blocks):
            for i in range(0, 12):
                if inode.blocks[i] == 0:
                    continue
                if inode.blocks[i] in allocated_block_nums:
                    add_message(inode.blocks[i], inode.inode_num, 0, 0)
                else :
                    allocated_block_nums.append(inode.blocks[i])
            for i in range(1,4):
                if inode.blocks[11 + i] == 0:
                    continue
                if inode.blocks[11 + i] in allocated_block_nums:
                    add_message(inode.blocks[i], inode.inode_num, i, offset_val[i])
                else :
                    allocated_block_nums.append(inode.blocks[11 + i])
    if row[0] == "DIRENT":
        directory = Directory(row)
        dirList.append(directory)
    if row[0] == "INDIRECT":
        indirect_block = IndirectBlock(row)
        if indirect_block.referenced_block in allocated_block_nums:
            add_message(indirect_block.block_num, indirect_block.parent_inode, indirect_block.level, offset_val[int(indirect_block.level)])
        else:
            allocated_block_nums.append(indirect_block.referenced_block)
    if row[0] == "BFREE":
        free_block_nums.append(int(row[1]))
    if row[0] == "IFREE":
        free_inode_nums.append(int(row[1]))

for block_num in range(8, s_block.num_blocks):
    if block_num not in free_block_nums and block_num not in allocated_block_nums:
        flaws = True
        print("UNREFERENCED BLOCK " + str(block_num))
    if block_num in free_block_nums and block_num in allocated_block_nums:
        flaws = True
        print("ALLOCATED BLOCK " + str(block_num) + " ON FREELIST")
for duplicates in duplicate_output_messages:
    print(duplicates)

    

for Inode in inodeList:
    counter = 0
    for pointer in Inode.blocks:
        counter = counter + 1
        if pointer < 0 or pointer > s_block.num_blocks:
            if counter in range(13,16):
                flaws = True
            if counter < 13:
                print "INVALID BLOCK", pointer, "IN INODE", Inode.inode_num, "AT OFFSET", (counter - 1)
            if counter == 13:
                print "INVALID INDIRECT BLOCK", pointer, "IN INODE",Inode.inode_num, "AT OFFSET 12"
            if counter == 14:
                print "INVALID DOUBLE INDIRECT BLOCK",pointer, "IN INODE", Inode.inode_num, "AT OFFSET 268"
            if counter == 15:
                print "INVALID TRIPLE INDIRECT BLOCK",pointer, "IN INODE", Inode.inode_num, "AT OFFSET 65804"


for Inode in inodeList:
    counter = 0
    for pointer in Inode.blocks:
        counter = counter + 1
        if pointer < 8 and pointer != 0:
            if counter in range(13,16):
                flaws = True
            if counter < 13:
                print "RESERVED BLOCK", pointer, "IN INODE", Inode.inode_num, "AT OFFSET", (counter - 1)
            if counter == 13:
                print "RESERVED INDIRECT BLOCK", pointer, "IN INODE",Inode.inode_num, "AT OFFSET 12"
            if counter == 14:
                print "RESERVED DOUBLE INDIRECT BLOCK",pointer, "IN INODE", Inode.inode_num, "AT OFFSET 268"
            if counter == 15:
                print "RESERVED TRIPLE INDIRECT BLOCK",pointer, "IN INODE", Inode.inode_num, "AT OFFSET 65804"

#MODIFIED FROM HERE

for inode in range(s_block.first_unreserved, s_block.num_inodes):
    if inode not in free_inode_nums and inode not in allocated_inode_nums:
        flaws = True
        print "UNALLOCATED INODE", inode, "NOT ON FREELIST"
    if inode in free_inode_nums and inode in allocated_inode_nums:
        flaws = True
        print "ALLOCATED INODE", inode, "ON FREELIST"
#MODIFIED TILL HERE




allocated_counter=0
for Inode in inodeList:
    if Inode.inode_num == allocated_inode_nums[allocated_counter]:
        links_counter = 0
        for dir in dirList:
            if dir.referenced_inode == Inode.inode_num:
              links_counter = links_counter+1
        if links_counter != Inode.link_count:
            flaws = True
            print "INODE", Inode.inode_num, "HAS", links_counter, "LINKS BUT LINKCOUNT IS", Inode.link_count
    allocated_counter = allocated_counter+1


for dir in dirList:
    unallocated_flag=0
    invalid_flag=0
    if dir.referenced_inode < 1 and dir.referenced_inode > s_block.num_inodes:
        invalid_flag=1
        flaws = True
        print "DIRECTORY INODE", dir.parent_inode, "NAME", dir.name, "INVALID INODE", dir.referenced_inode
    if dir.referenced_inode not in allocated_inode_nums:
        flaws = True
        unallocated_flag=1
        print "DIRECTORY INODE", dir.parent_inode, "NAME", dir.name, "UNALLOCATED INODE", dir.referenced_inode
    if unallocated_flag == 0 and invalid_flag == 0:
        if dir.name=="'..'":
            parent_to_child[dir.parent_inode] = dir.referenced_inode


for dir in dirList:
    if dir.name == "'.'":
        if dir.parent_inode != dir.referenced_inode:
            flaws = True
            print "DIRECTORY INODE", dir.parent_inode, "NAME", dir.name, "LINK TO INODE", dir.referenced_inode, "SHOULD BE", dir.parent_inode
    if dir.name == "'..'":
        if dir.referenced_inode != parent_to_child[dir.parent_inode]:
            flaws = True
            print "DIRECTORY INODE", dir.parent_inode, "NAME", dir.name, "LINK TO INODE", dir.referenced_inode, "SHOULD BE", parent_to_child[dir.parent_inode]

if flaws == True:
    sys.exit(2)
else :
    sys.exit(0)
