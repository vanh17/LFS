/*************************************************
 *
 *	filename: lfs.h
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
#include "fsmeta.h"
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include <fuse.h>
#include <string.h>
#include <fcntl.h>

#define FUSE_USE_VERSION 26
#ifndef LFS_H
#define LFS_H
#define num_args 3 // define the number of arguments we will pass to LFS main.
#define char_size sizeof(char)

#endif
