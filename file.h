/*************************************************
 *	filename: file.h
 *	CSC 552 LFS Project Phase 1
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 *	Hoang Van    vnhh@email.arizona.edu
 *  A file layer header of the LFS
 *************************************************/

#ifndef FILE_H
#define FILE_H
#define FUSE_USE_VERSION 26

#include "log.h"
#include "fsmeta.h"
#include <stdbool.h>

// size of address for allocation.
#define size_of_addr sizeof(struct addr)
// extern int s_blk_size; // block size - HV
// define size of inode for allocation purpose
#define size_of_inode sizeof(struct inode)

struct addr;
struct inode;

int make_file(int inum, int type);
void set_file_inode(struct inode *inode_inum, int inode_num, int ftype);
void read_file(int inum, int offset, int length, void *buffer);
void free_file(int inum);
void read_ifile(int inum_of_file, struct inode *inode_inum);
void write_ifile(int inum_of_file, struct inode *inode_inum);
int create_ifile_block(int inum, int blk_num);
int write_to_file(int inum, int offset, int length, void *buffer);
int write_data_to_block(struct inode *inode_inum, int block, int offset, int remaining_length, void *buffer, int isNewBlock);
#endif
