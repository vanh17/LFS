/*************************************************
 *
 *	filename: log.h
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A LFS layer header definition
 *************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "fsmeta.h"
#include "flash.h"

#define FUSE_USE_VERSION 26
#ifndef LOG_H
#define LOG_H

extern int size_seg_super_blk;
extern int nxt_tail_blk;


int write_to_flash();
int Log_Write(int inum, int block, int length, void* buffer, struct addr *logAddress);
void read_ifile(int inum_of_file, struct inode *inode_inum);
void write_ifile(int inum_of_file, struct inode *inode_inum);
void update_inode(int inum, int block, struct addr *block_addr, int length);
void update_summary_table(void* block, int block_num, uint16_t inum);
int Log_Free(struct addr *logAddress, int length);
void init_seg_summary(void* new_block);
int Log_Read(struct addr *logAddress, int length, void* buffer);
void get_addr(int inum, int i, struct addr *address);
#endif
