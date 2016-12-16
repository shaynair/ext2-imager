#ifndef __EXT2_IMAGER_H__
#define __EXT2_IMAGER_H__

/* Global Typedefs */
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char ubyte;
typedef enum { false, true } bool;

/* Global includes */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "ext2.h"

/* EXT2 Typedefs */
typedef struct ext2_super_block super_block;
typedef struct ext2_group_desc group_desc;
typedef struct ext2_inode inode;
typedef struct ext2_dir_entry_2 dir_entry;

/* General helpers */
#define BITS_PER_BYTE	8
#define	IS(i, b)	(i & b) != 0
#define DIV_UP(a, b)	((a + b - 1) / b)

/* Constants for EXT2 */
#define EXT2_SB_OFFSET	1024	/* Superblock offset */
#define EXT2_SB_SIZE	1024	/* Superblock size is constant */
#define EXT2_SIMPLE		128		/* Number of blocks in our simple file */

#define EXT2_DIR_DEFAULT_SIZE	8	/* How big a dir entry is, without name */
#define EXT2_ALIGN	4			/* Bytes to align directory entries */
#define EXT2_MIN_BLOCK_DATA		60	/* Maximum data for links without blocks */

#define EXT2_NUM_TYPES	4		/* Number of types of pointers in inode */
#define EXT2_NUM_SINGLE	12		/* Number of direct pointers */
#define EXT2_NUM_DOUBLE	1		/* Number of indirect pointers */
#define EXT2_NUM_TRIPLE	1
#define EXT2_NUM_QUAD	1

/* ext2_imager.c extern functions and variables  
  ------------------------------------------------- */
  
/* Global variables */

/* General helpers */
extern char *find_last_token(char *path);
extern bool has_space(uint inodes, uint blocks);

/* Loading and unloading */
extern bool load_simple_disk(char *file);
extern bool unload_disk(bool changed);

/* inode traversal */
extern inode *get_valid_inode(uint index);
extern uint find_direct_child(uint parent, char *file);
extern uint get_parent_inode_at_path(char *path);
extern uint get_inode_at_path_except(char *path, uint except);
extern uint get_inode_at_path(char *path);
extern uint get_inode_name_at_path(char *path, char *last_token);

/* Multi-purpose */
extern inode *new_inode(uint parent, uint num_blocks, ushort mode, char *name);
extern bool add_dir_entry(inode *p, uint index, ubyte file_type, char *name);
extern void write_file_data(inode *i, uint num_blocks, uint len, char *data);

/* for ls */
extern void print_dir_contents(uint curr, char *name, bool all);

/* for rm */
extern bool remove_entry(uint curr, char *name, inode *parent);

#endif 
/* __EXT2_IMAGER_H__ */