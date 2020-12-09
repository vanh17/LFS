/*************************************************
 *
 *	filename: lfsck.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A LFS layer header definition
 *************************************************/

#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "flash.h"
#include <errno.h>
#include "fsmeta.h"
#include "log.h"
#include "file.h"
#include <time.h>

#define LFS_ROOT_INUM 1

struct superblock *lfs_sb = NULL;
struct checkpoint_region *cp_region = NULL;
int s_block_byte = 0;
char *flash_file = NULL;

void print_superblock(struct superblock *sb)
{
	printf("--------------------- SB -------------------\n");
	printf("seg_size: %u\n", sb->seg_size);
	printf("b_size: %u\n", sb->b_size);
	printf("seg_num: %u\n", sb->seg_num);
	printf("sb_seg_num: %u\n", sb->sb_seg_num);
	printf("CP_addr_seg: %u\n", sb->checkpoint_addr.seg_num);
	printf("CP_addr_blo: %u\n", sb->checkpoint_addr.block_num);
	for(int ii=0;ii<sb->seg_size*sb->b_size*sb->seg_num;ii++)	{
		u_char      state;
		FlashGetState(flash, ii, &state);
		if (state == 1) {		// 1 means FLASH_STATE_FULL
			printf("%d sector is reserved\n",ii);
		}
	}
	printf("--------------------------------------------\n");
}

void print_inode(struct inode *inode)
{
	printf("\t----- inode -----\n");
	printf("\t inum: %u\n", inode->inum);
	printf("\t type: %u\n", inode->type);
	printf("\t size: %u\n", inode->size);
	printf("\t addr0: %u %u\n", inode->ptrs[0].seg_num, inode->ptrs[0].block_num);
	printf("\t addr1: %u %u\n", inode->ptrs[1].seg_num, inode->ptrs[1].block_num);
	printf("\t addr2: %u %u\n", inode->ptrs[2].seg_num, inode->ptrs[2].block_num);
	printf("\t addr3: %u %u\n", inode->ptrs[3].seg_num, inode->ptrs[3].block_num);
	printf("\t-----------------\n");
}

void print_cp_region()
{
	printf("---------- CP region ----------\n");
	printf("timestamp: %ld\n", cp_region->timestamp);
	printf("LSA-seg_num: %u\n", cp_region->last_seg_addr.seg_num);
	printf("LSA-block: %u\n", cp_region->last_seg_addr.block_num);
	printf("ifile-inode-inum: %u\n", cp_region->ifile_inode.inum);
	printf("ifile-inode-type: %u\n", cp_region->ifile_inode.type);
	printf("ifile-inode-size: %d\n", cp_region->ifile_inode.size);
	printf("ifile-inode-addr-seg: %u\n", cp_region->ifile_inode.ptrs[0].seg_num);
	printf("ifile-inode-addr-block: %u\n", cp_region->ifile_inode.ptrs[0].block_num);
	printf("-------------------------------\n");
}

int segNum_To_Sectors(uint16_t seg_num) {
	return seg_num * lfs_sb->seg_size * lfs_sb->b_size;
}
int logAddr_To_Sectors(struct addr *addr) {
	return addr->seg_num * segNum_To_Sectors(1)
			+ addr->block_num * lfs_sb->b_size;
}
int LFS_SEG(int x) {
	return x + super_blk_size;
}
void load_segment_usage_table() {
	seg_hash_tab = malloc(lfs_sb->seg_num);
	Log_Read(&(lfs_sb->seg_usage_table_addr), lfs_sb->seg_num, seg_hash_tab);
}

int main(int argc, char *argv[])
{
	*flash_file = argv[argc-1];
	u_int *n_blocks = malloc(sizeof(u_int));
	flash = Flash_Open(flash_file, FLASH_ASYNC, n_blocks);

	// 1. test for mklfs layer
	// 1-1. print superblock
	lfs_sb = malloc(sizeof(struct superblock));

	void *buffer = malloc(FLASH_SECTOR_SIZE * 64);
	if(Flash_Read(flash, 0, 64, buffer) == 1)
		printf("ERROR: %s\n", strerror(errno));
	uint16_t *seg_size = (uint16_t*) (buffer);
	uint16_t *b_size = (uint16_t*) (buffer + sizeof(uint16_t));
	uint16_t *sb_seg_num = (uint16_t*) (buffer + sizeof(uint16_t) * 3);
	int sectors = *seg_size * *b_size * *sb_seg_num;
	void *temp = malloc(sectors * FLASH_SECTOR_SIZE);
	Flash_Read(flash, 0, sectors, temp);
	memcpy(lfs_sb, temp, sizeof(struct superblock));
	print_superblock(lfs_sb);
	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;


	// 1-2. print checkpoint
	cp_region = malloc(sizeof(struct checkpoint_region));
	// Log_Read(&(lfs_sb->checkpoint_addr), sizeof(struct checkpoint_region), cp_region);

	u_int sector_offset = logAddr_To_Sectors(&(lfs_sb->checkpoint_addr));
	// before calling Flash_Read, need to get buffer in Flash_Read enough space to read in.
	u_int sector_n = sizeof(struct checkpoint_region) / FLASH_SECTOR_SIZE + (sizeof(struct checkpoint_region) % FLASH_SECTOR_SIZE == 0 ? 0 : 1);
	void *temp_t = malloc(FLASH_SECTOR_SIZE * sector_n);
	if(Flash_Read(flash, sector_offset, sector_n, temp_t) == 1)
		printf("ERROR: %s\n", strerror(errno));
	memcpy(cp_region, temp_t, sizeof(struct checkpoint_region));

	print_cp_region();


	// 2. test for log layer
	tail_seg = malloc(sizeof(struct segment));
	tail_seg->seg_num = cp_region == NULL ? LFS_SEG(0) : cp_region->last_seg_addr.seg_num + 1;
	tail_seg->blocks = malloc(lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE);
	memset(tail_seg->blocks, 0, lfs_sb->seg_size * lfs_sb->b_size * FLASH_SECTOR_SIZE); // clean the memory

	s_block_byte = lfs_sb->b_size * FLASH_SECTOR_SIZE;
	uint16_t size_seg_summary = lfs_sb->seg_size / (s_block_byte / sizeof(uint16_t));
	if((lfs_sb->seg_size % (s_block_byte / sizeof(uint16_t)) != 0)) size_seg_summary ++;
	uint16_t next_block_in_tail = size_seg_summary;

	// 2-0. CHECK ROOT DIR
	struct inode *root_inode = malloc(sizeof(struct inode));
	read_ifile(LFS_ROOT_INUM, root_inode);
	load_segment_usage_table();
	printf("root??? inode->inum : %u , inode->type : %u \n", root_inode->inum, root_inode->type);

	// 2-1. Log_Write (less than 1 segment :: validation check)
	char* block_data = malloc(sizeof(char) * 10);
	strcpy(block_data, "Hello!");
	struct addr * address = malloc(sizeof(struct addr));
	Log_Write(2, 1, 7, block_data, address);

	// 2-2. Log_Write (append :: write_tail_seg_to_flash)
	// 2-3. Log_Read 2-1 and 2-2 (from log and tail_segment)
	u_char      state;
	FlashGetState(flash, 2 * lfs_sb->seg_size * lfs_sb->b_size, &state);
	if (state == 1) {		// 1 means FLASH_STATE_FULL
		printf("%d sector is reserved\n",2 * lfs_sb->seg_size * lfs_sb->b_size);
	}

	void* read_buffer = malloc(sizeof(char) * 10);
	address->seg_num = 2;
	address->block_num = 1;
	Log_Read(address, 10, read_buffer);

	printf("read_buffer : %s, %l @address %u %u\n\n", read_buffer, read_buffer, address->seg_num, address->block_num);
	// 2-4. unmount (update segment_usage_table and checkpoint)
	// 2-5. mount
	// 2-6. Log_Read 2-1 and 2-2 again (same as 2-3 ?)
	// 2-7. Log_Free 2-1 and 2-2 (init?)
	address->block_num = 0;
	int lFree_return = Log_Free(address, 32*1024);
	printf("Log_Free returned %d\n\n",lFree_return);
	free(address);

	// 3. test for file layer
	// 3-1. test for File_Create
	// 3-2. test for File_Write (more than 1 segment)
	// 3-3. test for File_Read 3-2
	// 3-4. test for File_Write (rewrite)
	// 3-5. test for File_Free

	return 0;
}
