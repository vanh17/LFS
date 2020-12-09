/*************************************************
 *
 *	filename: cleaner.h
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
#ifndef CLEANER_H
#define CLEANER_H

// extern int size_seg_super_blk;


int start_cleaner();
void need_to_clean(int seg_num, int* ntc);
uint16_t get_block_inum(void* segment, int block_num);
int is_fragmented(int inum, struct addr *seg_addr);
int is_same_addresses(struct addr *addr1, struct addr *addr2);
void init_segment_summary_table(int block_num);
void append_block_to_clean_segment(void* block, int inode_num, int old_seg_num, int old_block_num);
#endif
