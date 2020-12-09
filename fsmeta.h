/**
 *	filename: fsmeta.c
 *	CSC 552 LFS Project Phase 1
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 *	Hoang Van    vnhh@email.arizona.edu
 */

#define FUSE_USE_VERSION 26
#ifndef FSMETA_H
#define FSMETA_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "flash.h"

extern Flash flash;
extern struct superblock *super_blk_lfs; // added global variable super_blk_lfs -HV
extern struct segment *tail_seg;
extern int size_seg;
extern int size_seg_super_blk;
extern char *flash_file;
extern int s_blk_size; // HV updated the s_blk_size
extern int max_cache_segment; //added by HV
extern void *seg_hash_tab;
extern int avail_seg_ctr;
extern struct checkpoint_region *chk_ptr; // added global for chkpoint
extern int cp_range;		// shkim 1120. ckpt interval
extern int start;			// shkim 1120. cleaner
extern int stop;			// shkim 1120. cleaner

// define constansts here for eliminate duplications.
#define AVAIL_ADDR 0xFFFF /* available address in LFS layer - HV set it to 0xFFFF to passed tests on @Lec*/
#define BLOCK_UNUSED 0
#define BLOCK_DATA 1
#define SEG_UNUSED 0		// shkim 1121. cleaner
#define SEG_USED 1		// shkim 1121. cleaner
#define LFS_CP_REGION_INUM 2
//#define BLOCK_IFILE 2
#define super_blk_size sizeof(struct superblock)
#define uint16_t_size sizeof(uint16_t)
#define size_of_inode sizeof(struct inode)
#define size_of_chk_ptr sizeof(struct checkpoint_region)
#define size_of_seg sizeof(struct segment)
#define uint_size sizeof(u_int)
#define size_of_dir_entry sizeof(struct dir_entry)
#define size_of_dir sizeof(struct dir)
#define size_of_addr sizeof(struct addr)


// define type used for this assignment.

struct addr{
	uint16_t seg_num;						// segment number
	uint16_t block_num;						// block number within the segment
};

struct inode{
	uint16_t n_links;						// # of hard links
	int size;								// size of the file.
	mode_t mode;							// mode of the file
	uint16_t type;							// type of the file, file or directory
	time_t lst_acs;							// last accessed time	// shkim
	uid_t uid;								// user id
	time_t lst_mdf;							// last modification time
	char padding[6];
	gid_t gid; 								// group id
	struct addr ptrs[4];
	struct addr indirect;					// Pointer to indirect block
	time_t lst_cre;							// last created time	// shkim
	uint16_t inum;							// inum
};

struct checkpoint_region{
	uint32_t segment_usage_table;			// PLACEHOLDER for segment usage table.
	struct addr last_seg_addr;				// last segment written
	struct inode ifile_inode;				// inode of THE ifile
	time_t timestamp;						// timestamp, time_t is uint64_t
	uint32_t next_inum;						// the next available inum in the system.
};

struct superblock {
	uint16_t sb_seg_num;					// # of segments for superblock usage
	uint16_t seg_size; 						// segment size, in blocks
	struct addr seg_usage_table_addr;		// address of the segment usuage table address
	uint16_t seg_num;						// # of segments
	struct addr checkpoint_addr;			// address of the current(most recent) checkpoint region
	uint16_t b_size;						// block size, in sectors
};

struct segment{
	uint16_t seg_num;				// the segment number of this segment
	void *blocks;					// blocks in 1 segment
};


// function declarations
void update_chk_ptr();
void fsmeta_init();
void super_blk_init();
void usage_seg_tab_init();
void update_segment_usage_table(int seg_num, int isUnvailble);
void update_sb();
#endif
