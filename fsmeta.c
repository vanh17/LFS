/*************************************************
 *
 *	filename: fsmeta.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *************************************************/
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "fsmeta.h"
#include "lfs.h"
#include "log.h"
#include <unistd.h>

int size_seg_super_blk;
extern int size_seg;
extern int s_blk_size;


/***********************************************************************
 * function name: update_chk_ptr
 * what it does? a helper to help update the checkpoint so that we can 
 * save update to the current check point
 * parameter(0)
 * return: nothing - HV
 ***********************************************************************/
void update_chk_ptr(){
	printf("update_chk_ptr \n\n");
	// 1. write the most recent checkpoint region.
	chk_ptr->last_seg_addr.seg_num = tail_seg->seg_num;
	void *cp_buffer = malloc( s_blk_size );
	memcpy(cp_buffer, chk_ptr,  size_of_chk_ptr  );
	// block=0 is an exception for the situation.
	Log_Write(-1, 0,  s_blk_size , cp_buffer, &(super_blk_lfs->checkpoint_addr));
	// now after log write, we need to write changes made to the seg hash table.
	int size_seg_hash_tab = sizeof(seg_hash_tab);
	int start_sector = (super_blk_lfs->seg_usage_table_addr.seg_num * super_blk_lfs->seg_size
			+ super_blk_lfs->seg_usage_table_addr.block_num) * super_blk_lfs->b_size;
	int extra_sector = 0; // how much num_sector has to change
	if (size_seg_hash_tab % FLASH_SECTOR_SIZE != 0) {
		extra_sector = 1; // if we need more sector set this to 1
	}
	int num_sector = size_seg_hash_tab / FLASH_SECTOR_SIZE + extra_sector;
	// write to flash for phase 1 will have to update this on phase2? May be ask John
	Flash_Write(flash, start_sector, num_sector, seg_hash_tab);
	void *sb_buffer = malloc(super_blk_lfs->seg_size *  s_blk_size ); //allocate the buffer for write
	memcpy(sb_buffer, super_blk_lfs,  super_blk_size ); // load it to mem
	// clear out the flash at that position
	Flash_Erase(flash, 0, super_blk_lfs->seg_size * super_blk_lfs->b_size / FLASH_SECTORS_PER_BLOCK);
	// update the content to flash.
	Flash_Write(flash, 0, super_blk_lfs->seg_size * super_blk_lfs->b_size, sb_buffer);
}

/*
 *----------------------------------------------------------------------
 * function name: super_blk_init
 * What it does? initialize instance of super block.
 * Parameters(0)
 * Return: nothing - hv
 *----------------------------------------------------------------------
 */
void super_blk_init() {
	super_blk_lfs = malloc( super_blk_size ); // the very first time we create the super_blk
	// then we need allocate the buffer for read and write purposes
	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	Flash_Read(flash, 0, 64, buffer); // read from Flash
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer +  uint16_t_size );
	uint16_t *sb_seg_num = (uint16_t*) (buffer +  uint16_t_size * 3);
	int num_sec = *seg_size * *b_size * *sb_seg_num; //number of sectors for this super block.
	void *temp_buffer = malloc(num_sec * FLASH_SECTOR_SIZE);
	Flash_Read(flash, 0, num_sec, temp_buffer); // now read flash to temporay buff
	memcpy(super_blk_lfs, temp_buffer,  super_blk_size ); //update to mem
}

/*
 *----------------------------------------------------------------------
 * function name: fsmeta_init
 * What it does? This function initialize all the data structures that our LFS will use, including
 *reading from the flash about super block and the checkpoint region.
 * Parameters(0)
 * Return: nothing
 *----------------------------------------------------------------------
 */
void fsmeta_init() {
	u_int *num_blks = malloc(uint_size);
	flash = Flash_Open(flash_file, FLASH_ASYNC, num_blks);
	super_blk_init(); // initialize the super block for this fsmeta instance
	s_blk_size  = super_blk_lfs->b_size * FLASH_SECTOR_SIZE;
	size_seg = super_blk_lfs->seg_size / ( s_blk_size  /  uint16_t_size );
	int size_of_one_seg = ( s_blk_size  /  uint16_t_size );
	if((super_blk_lfs->seg_size %  size_of_one_seg != 0)) {
		size_seg = size_seg+1; // if we need more than one seg, add 1 to it.
	}
	size_seg_super_blk = super_blk_lfs->sb_seg_num; // init super block size of segment 
	chk_ptr = malloc( size_of_chk_ptr  ); //init the checkpoint region for fsmeta structure
	int num_sectors = (&(super_blk_lfs->checkpoint_addr))->seg_num * super_blk_lfs->seg_size * super_blk_lfs->b_size 
						+ (&(super_blk_lfs->checkpoint_addr))->block_num * super_blk_lfs->b_size; 
	u_int sec_offset = num_sectors;
	u_int sec_n =  size_of_chk_ptr / FLASH_SECTOR_SIZE;
	if (size_of_chk_ptr%FLASH_SECTOR_SIZE != 0) {
		sec_n += 1; // need to increase the num sectors.
	}
	void *temp_buffer = malloc(FLASH_SECTOR_SIZE * sec_n);
	Flash_Read(flash, sec_offset, sec_n, temp_buffer); // read from flash and put it to temp_buffer
	memcpy(chk_ptr, temp_buffer,  size_of_chk_ptr  ); // update to memory
	tail_seg = malloc( size_of_seg );
	tail_seg->seg_num = chk_ptr == NULL ? size_seg_super_blk : chk_ptr->last_seg_addr.seg_num + 1; 
	tail_seg->blocks = malloc(super_blk_lfs->seg_size * super_blk_lfs->b_size * FLASH_SECTOR_SIZE);
	memset(tail_seg->blocks, 0, super_blk_lfs->seg_size * super_blk_lfs->b_size * FLASH_SECTOR_SIZE); // erease what on mem
	init_seg_summary(tail_seg->blocks);		// shkim 1121
	usage_seg_tab_init(); //initialize the segment table

}

/*
 *----------------------------------------------------------------------
 * function name: usage_seg_tab_init
 * what it does? load the segment usage_table
 * parameter(0)
 * return nothing - hv
 *----------------------------------------------------------------------
 */
void usage_seg_tab_init() {
	// 	num_of_block_seg_usage_table represents number of blocks being reserved for segment usage table
	seg_hash_tab = malloc(super_blk_lfs->seg_num);
	Log_Read(&(super_blk_lfs->seg_usage_table_addr), super_blk_lfs->seg_num, seg_hash_tab);
	for (int i = size_seg_super_blk; i < super_blk_lfs->seg_num; i++) {
		uint8_t *temp = seg_hash_tab + i;
		if (*temp == 0) { // update available segment - HV
			avail_seg_ctr = avail_seg_ctr+1;
		}
	}
}


/******************************************************************
 * this is just a helper
 ******************************************************************/
void update_segment_usage_table(int seg_num, int isUnvailble) {
	memset(seg_hash_tab + seg_num, isUnvailble, sizeof(uint8_t));
}

/*
 *----------------------------------------------------------------------
 * Associative Functions
 * This part consists of functions that are used for general-purpose
 * this is also a helper.
 *----------------------------------------------------------------------
 */
void update_sb() {
	Flash_Write(flash, 0, super_blk_lfs->seg_size * super_blk_lfs->b_size * super_blk_lfs->seg_num, super_blk_lfs);
}

