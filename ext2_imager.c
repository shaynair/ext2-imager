#include "ext2_imager.h"

/* Constants */
const char DELIMITER[2] = "/";
const uint TYPES[EXT2_NUM_TYPES] = {EXT2_NUM_SINGLE, EXT2_NUM_DOUBLE,
								EXT2_NUM_TRIPLE, EXT2_NUM_QUAD};

/* Constant variables */
ubyte *disk = NULL;
super_block *sb = NULL;
group_desc *gd = NULL;
int fd = -1;
uint curr_time = -1;

/* Constant methods */				

/* Converts a inode file type to a dir entry file type. */	
static ubyte to_dir_type(ushort type) {
	switch(type) {
		case EXT2_S_IFLNK:
			return EXT2_FT_SYMLINK;
		case EXT2_S_IFDIR:
			return EXT2_FT_DIR;
		case EXT2_S_IFREG:
			return EXT2_FT_REG_FILE;
		default:
			return EXT2_FT_UNKNOWN;
	}
}

/* Opens the disk image file and maps it into memory.
 * Also initializes the superblock and group descriptors.
 * Return true on success.
 */
bool load_simple_disk(char *file) {
	if ((fd = open(file, O_RDWR)) < 0) {
		perror("open");
		return false;
	}
    disk = mmap(NULL, EXT2_SIMPLE * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, 
								MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
		perror("mmap");
		return false;
    }
	
	sb = (super_block *)(disk + EXT2_SB_OFFSET);
	gd = (group_desc *)(disk + EXT2_SB_OFFSET + EXT2_SB_SIZE);
	
	curr_time = (uint)time(NULL);
	
	return true;
}

/* Return true if we have space for inodes and blocks. */
bool has_space(uint inodes, uint blocks) {
	assert (disk != NULL);
	
	return gd->bg_free_blocks_count >= blocks && 
				gd->bg_free_inodes_count >= inodes;
}

/* Returns a pointer to the block specified by index. */
ubyte *get_block(uint index) {
	assert (disk != NULL);
	
	return (disk + (index * EXT2_BLOCK_SIZE));
}

/* Gets whether a block is used or not.
 * Return true if it is (in the bitmap), false otherwise.
 */
bool get_block_bitmap(uint index) {
	ubyte *bitmap;
	
	/* Make sure initialized and correct block index */
	assert(disk != NULL && index > 0 && index <= sb->s_blocks_count);
	
	bitmap = get_block(gd->bg_block_bitmap);
	index--; /* Starts from 1 */
	return (bitmap[index / BITS_PER_BYTE] & (1 << (index % BITS_PER_BYTE))) != 0;
}

/* Sets whether a block is used or not, in the bitmap. */
void set_block_bitmap(uint index, bool set) {
	ubyte *bitmap;
	
	/* Make sure initialized and correct block index */
	assert(disk != NULL && index > 0 && index <= sb->s_blocks_count);
	
	bitmap = get_block(gd->bg_block_bitmap);
	index--; /* Starts from 1 */
	if (set) {
		bitmap[index / BITS_PER_BYTE] |= (1 << (index % BITS_PER_BYTE));
	} else {
		bitmap[index / BITS_PER_BYTE] &= ~(1 << (index % BITS_PER_BYTE));
	}
}

/* Returns a pointer to the valid block specified by index. */
ubyte *get_valid_block(uint index) {
	assert (get_block_bitmap(index));
	
	return get_block(index);
}

/* Gets whether a inode is used or not.
 * Return true if it is (in the bitmap), false otherwise.
 */
bool get_inode_bitmap(uint index) {
	ubyte *bitmap;
	
	/* Make sure initialized and correct inode index */
	assert(disk != NULL && index > 0 && index <= sb->s_inodes_count);
	
	bitmap = get_block(gd->bg_inode_bitmap);
	index--;
	return (bitmap[index / BITS_PER_BYTE] & (1 << (index % BITS_PER_BYTE))) != 0;
}

/* Sets whether a inode is used or not, in the bitmap. */
void set_inode_bitmap(uint index, bool set) {
	ubyte *bitmap;
	
	/* Make sure initialized and correct inode index */
	assert(disk != NULL && index > 0 && index <= sb->s_inodes_count);
	
	bitmap = get_block(gd->bg_inode_bitmap);
	index--;
	if (set) {
		bitmap[index / BITS_PER_BYTE] |= (1 << (index % BITS_PER_BYTE));
	} else {
		bitmap[index / BITS_PER_BYTE] &= ~(1 << (index % BITS_PER_BYTE));
	}
}

/* Finds an unused block index.
 * Return 0 if none exist.
 */
uint find_free_block() {
	uint i;
	
	if (!has_space(0, 1)) {
		return 0;
	}
	
	for (i = 1; i <= sb->s_blocks_count; i++) {
		if (!get_block_bitmap(i)) {
			return i;
		}
	}
	return 0;
}

/* Initializes or uninitializes a block for an inode. */
void initialize_block(uint index, inode *i, bool init) {
	ubyte *ptr = get_block(index);
	
	/* Make sure it was properly set */
	assert(init != get_block_bitmap(index));
	
	memset(ptr, 0, EXT2_BLOCK_SIZE);
	
	gd->bg_free_blocks_count += (init ? -1 : 1);
	sb->s_free_blocks_count += (init ? -1 : 1);
	set_block_bitmap(index, init);
	
	i->i_size += EXT2_BLOCK_SIZE * (init ? 1 : -1);
	i->i_blocks += 2 * (init ? 1 : -1); /* sectors */
	i->i_mtime = curr_time;
	
	if (init) {
		/* Set a directory entry */
		((dir_entry *)ptr)->rec_len = EXT2_BLOCK_SIZE;
	}
}

/* Finds an unused inode index.
 * Return 0 if none exist.
 */
uint find_free_inode() {
	uint i;
	
	if (!has_space(1, 0)) {
		return 0;
	}
	
	for (i = 1; i <= sb->s_inodes_count; i++) {
		if (!get_inode_bitmap(i)) {
			return i;
		}
	}
	return 0;
}

/* Gets the inode at the index provided. */
inode *get_inode(uint index) {
	/* Accessing a bad (reserved) inode */
	assert(disk != NULL && index > 0 && index <= sb->s_inodes_count
				&& (index >= sb->s_first_ino || index == EXT2_ROOT_INO));
	
	return (inode *)(get_block(gd->bg_inode_table)
						+ (sb->s_inode_size * (index - 1)));
}

/* Gets whether the inode is a created entry or not. */
bool is_inode_valid(inode *ptr) {
	return ptr->i_ctime > 0 && ptr->i_dtime == 0;
}

/* Gets the valid inode at the index provided. */
inode *get_valid_inode(uint index) {
	inode *ret = get_inode(index);
	if (ret != NULL && get_inode_bitmap(index) && is_inode_valid(ret)) {
		ret->i_atime = curr_time; /* Access time */
		return ret;
	}
	return NULL;
}

/* Takes any existing blocks referred to by an inode and deallocates them. */
void unset_inode_block(uint block, inode *i) {
	initialize_block(block, i, false);
}

/* Finds the last entry in a path delimited by '/' */
char *find_last_token(char *path) {
	return basename(path);
}

/* Checks if the directory entry's name is equal to name. */
bool check_dir_entry_name(dir_entry *dir, char *name) {
	return dir->name_len == strlen(name)
				&& strncmp(dir->name, name, dir->name_len) == 0;
}

/* Checks if the entry provided is a special one. */
bool is_special_dir(dir_entry *entry) {
	return check_dir_entry_name(entry, ".") || check_dir_entry_name(entry, "..");
}

/* If f is NULL, gets the directory entry with the 
 * filename equal to file in this block.
 *
 * Otherwise, if f is not NULL, performs function f on the 
 * directory entries. If file is not NULL, only performs f on
 * the entry with filename equal to file.
 *
 * Also performs g on all blocks pointed to by inode.
 * If get_prev is true, returns the directory entry previous.
 */
dir_entry *search_inner_block(inode *curr, uint index, void (*g)(uint, inode *), 
				char *file, uint recurse, void (*f)(dir_entry *, inode *), bool get_prev) {
	ubyte *block;
	uint *ptr, i, temp;
	dir_entry *ret, *prev = NULL;
	
	if (!get_block_bitmap(index)) { /* Invalid block */
		return NULL;
	}
	block = get_valid_block(index);
	
	if (recurse == 0) {
		if (g != NULL) {
			g(index, curr);
		}
		if (IS(curr->i_mode, EXT2_S_IFDIR)) {
			/* Direct pointers, sequentially read */
			for (i = 0; i < EXT2_BLOCK_SIZE; i += temp) {
				ret = (dir_entry *)(block + i);
				temp = ret->rec_len; /* In case we get wiped */
				if (temp < (uint)EXT2_DIR_DEFAULT_SIZE + ret->name_len) {
					/* Block was deallocated while we were going through */
					return NULL;
				}
				
				/* If valid and follows conditions */
				if (ret->inode > 0 && get_valid_inode(ret->inode) != NULL 
						&& (file == NULL || check_dir_entry_name(ret, file))) {
					if (f != NULL) {
						f(ret, curr);
					} else {
						return (get_prev ? prev : ret);
					}
				}
				/* f may have wiped this entry */
				if (ret->rec_len > 0) {
					temp = ret->rec_len;
					prev = ret;
				}
			}
		}
	} else {
		/* Indirect pointers, recurse */
		for (i = 0; i < EXT2_BLOCK_SIZE / sizeof(uint); i++) {
			ptr = (uint *)(block + (i * sizeof(uint)));
			if (ptr == NULL || *ptr == 0) { /* All remaining pointers are 0 */
				return NULL;
			}
			ret = search_inner_block(curr, *ptr, g, file, recurse - 1, 
												f, get_prev);
			if (ret != NULL) {
				return ret;
			}
		}
	}
	return NULL;
}

/* If f is NULL, gets the inode with the 
 * filename equal to file as a child of the inode provided.
 *
 * Otherwise, if f is not NULL, performs function f on the 
 * directory entries. If file is not NULL, only performs f on
 * the entries with filename equal to file.
 *
 * Also performs g on all blocks pointed to by inode.
 */
uint perform_on_children(inode *curr, void (*g)(uint, inode *), char *file,
						void (*f)(dir_entry *, inode *)) {
	uint i, j;
	uint min, block;
	dir_entry *ret;
	
	/* Can't all be NULL */
	assert(f != NULL || g != NULL || file != NULL);
	
	min = 0;
	for (j = 0; j < EXT2_NUM_TYPES; j++) {
		for (i = 0; i < TYPES[j]; i++) {
			block = curr->i_block[i + min];
			if (block == 0) { /* Remaining are 0 */
				return 0;
			}
			ret = search_inner_block(curr, block, g, file, j, f, false);
			if (ret != NULL) { 
				/* Found filename */
				return ret->inode;
			}
		}
		min += i;
	}
	
	return 0;
}

/* Initializes or uninitializes an inode. 
 * Note that if inode is a directory, the directory entries
 * should be removed BEFORE calling this method.
 */
void initialize_inode(uint index, bool init) {
	inode *i = get_inode(index);
	
	/* Make sure it was properly set */
	assert(init != get_inode_bitmap(index));
	
	gd->bg_free_inodes_count += (init ? -1 : 1);
	sb->s_free_inodes_count += (init ? -1 : 1);
	set_inode_bitmap(index, init);
	
	/* Unset the blocks */
	if (i->i_size < EXT2_MIN_BLOCK_DATA && IS(i->i_mode, EXT2_S_IFLNK)) {
		/* Special case */
		memset(i->i_block, 0, i->i_size);
	} else {
		perform_on_children(i, unset_inode_block, NULL, NULL);
	}
	
	if (init) {
		memset(i, 0, sb->s_inode_size);
		i->i_ctime = curr_time;
		i->i_dtime = 0;
	} else {
		if (IS(i->i_mode, EXT2_S_IFDIR)) {
			/* One less used directory */
			gd->bg_used_dirs_count--;
		}
		i->i_dtime = curr_time;
	}
	i->i_mtime = curr_time;
	i->i_atime = curr_time;
}

/* Reads block contents to an index in a C-style string. 
 * Return true if the string has ended.
 */
bool read_block_contents(uint index, char *dest, uint *start, 
						uint *remaining, uint recurse) {
	ubyte *block;
	uint i, *ptr;
	uint size = *remaining;
	
	if (index == 0 || !get_block_bitmap(index) || size == 0) {
		return true;
	}

	block = get_valid_block(index);
	
	if (recurse == 0) {
		if (size > EXT2_BLOCK_SIZE) {
			size = EXT2_BLOCK_SIZE; /* Amount of bytes to read for this block */
		}
		strncpy(dest, (char *)(block + (*start * sizeof(char))), size);
		*remaining -= size; /* Decrease how many bytes remaining we have */
		*start += size; /* Advance pointer to write */
	} else {
		/* Indirect pointers, recurse */
		for (i = 0; i < EXT2_BLOCK_SIZE / sizeof(uint); i++) {
			ptr = (uint *)(block + (i * sizeof(uint)));
			if (read_block_contents(*ptr, dest, start, remaining, recurse - 1)) {
				return true;
			}
		}
	}
	return *remaining == 0;
}


/* Reads file contents to a C-style string. */
char *read_file_contents(uint index) {
	inode *node = get_valid_inode(index);
	char *ret;
	uint i, j;
	uint min, remaining, start;
	
	/* Is not a directory */
	assert(!IS(node->i_mode, EXT2_S_IFDIR));
	
	ret = malloc(node->i_size + sizeof(char));
	
	if (node->i_size < EXT2_MIN_BLOCK_DATA && IS(node->i_mode, EXT2_S_IFLNK)) {
		/* Special case: short symlinks read directly from block */
		strncpy(ret, (char *)node->i_block, node->i_size);
	} else {
		/* Look through blocks and get the contents */
		min = 0;
		start = 0; /* Index of where to write */
		remaining = node->i_size;
		for (j = 0; j < EXT2_NUM_TYPES; j++) {
			for (i = 0; i < TYPES[j]; i++) {
				/* Stop when we end the string */
				if (read_block_contents(node->i_block[i + min], ret, 
									&start, &remaining, j)) {
					j = EXT2_NUM_TYPES;
					/* Break outer loop */
					break;
				}
			}
			min += i;
		}
	}
	ret[node->i_size] = '\0';
	return ret;
}

/* Returns a directory entry that has the extra space needed. */
dir_entry *get_free_dir_entry(uint index, uint size_needed) {
	ubyte *block;
	uint i;
	uint space;
	dir_entry *ret;
	
	/* Round up to ALIGN bytes */
	size_needed += (EXT2_ALIGN - (size_needed % EXT2_ALIGN));
	
	/* Direct pointers, sequentially read */
	block = get_valid_block(index);
	for (i = 0; i < EXT2_BLOCK_SIZE; i += ret->rec_len) {
		ret = (dir_entry *)(block + i);
		
		space = EXT2_DIR_DEFAULT_SIZE + ret->name_len; /* Space expected */
		space += (EXT2_ALIGN - (space % EXT2_ALIGN));
		
		/* Has enough space for both this dir entry and new one */
		if (ret->rec_len >= space + size_needed) {
			return ret;
		}
	}
	return NULL;
}

/* Returns true if the block has room for a directory entry. */
bool has_free_dir_entry(uint index, uint size_needed) {
	return get_free_dir_entry(index, size_needed) != NULL;
}

/* Return true if the block given meets conditions of check. */
bool has_free_entry(uint index, uint size_needed, uint recurse, 
								bool (*check)(uint, uint)) {
	ubyte *block;
	uint i, *ptr;
	uint block_index;

	block = get_valid_block(index);
	
	if (recurse == 0) {
		if (check != NULL && check(index, size_needed)) {
			return true;
		}
	} else {
		/* Indirect pointers, recurse */
		for (i = 0; i < EXT2_BLOCK_SIZE / sizeof(uint); i++) {
			ptr = (uint *)(block + (i * sizeof(uint)));
			block_index = *ptr;
			if (block_index != 0 && !get_block_bitmap(block_index)) {
				/* Invalid block, we can use this pointer */
				*ptr = 0;
				block_index = 0;
			}
			if (block_index == 0 
					|| has_free_entry(block_index, size_needed, recurse - 1, check)) {
				return true;
			}
		}
	}
	return false;
}

/* Returns a pointer to the first i_block entry in an inode
 * which meets the conditions given by check function.
 */
uint *get_free_block_ptr(inode *curr, uint size_needed, bool (*check)(uint, uint)) {
	uint i, j;
	uint min, block;

	min = 0;
	for (j = 0; j < EXT2_NUM_TYPES; j++) {
		for (i = 0; i < TYPES[j]; i++) {
			block = curr->i_block[i + min];
			if (block != 0 && !get_block_bitmap(block)) {
				/* Invalid block, we can use this pointer */
				curr->i_block[i + min] = 0;
				block = 0;
			}
			if (block == 0 || has_free_entry(block, size_needed, j, check)) {
				return &(curr->i_block[i + min]);
			}
		}
		min += i;
	}
	
	return NULL;
}


/* Gets a direct block index that contains a directory entry with name. */
uint search_indirect_block(inode *curr, uint index, char *name, uint recurse) {
	ubyte *block;
	uint *ptr, i, ret;
	
	if (!get_block_bitmap(index)) { /* Invalid block */
		return 0;
	}
	
	if (recurse == 0) {
		if (search_inner_block(curr, index, NULL, name, 0, NULL, false) != NULL) {
			return index;
		}
	} else {
		block = get_valid_block(index);
		/* Indirect pointers, recurse */
		for (i = 0; i < EXT2_BLOCK_SIZE / sizeof(uint); i++) {
			ptr = (uint *)(block + (i * sizeof(uint)));
			if (ptr == NULL || *ptr == 0) { /* All remaining pointers are 0 */
				return 0;
			}
			ret = search_indirect_block(curr, *ptr, name, recurse - 1);
			if (ret != 0) {
				return ret;
			}
		}
	}
	return 0;
}

/* Returns a block index containing the directory entry with name. */
uint get_block_with_entry(inode *curr, char *name) {
	uint i, j;
	uint min, block;

	min = 0;
	for (j = 0; j < EXT2_NUM_TYPES; j++) {
		for (i = 0; i < TYPES[j]; i++) {
			block = curr->i_block[i + min];
			if (block == 0) {
				return 0; /* Remaining are 0 */
			}
			block = search_indirect_block(curr, block, name, j);
			if (block != 0) {
				return block;
			}
		}
		min += i;
	}
	
	return 0;
}

/* Finds and allocates a new block and adds it to inode. 
 * Returns the block index if successful. Otherwise, return 0.
 */
uint add_new_block_to_inode(inode *i) {
	uint new_block = find_free_block();
	uint *block_ptr = get_free_block_ptr(i, 0, NULL);
	if (block_ptr == NULL || new_block == 0) {
		return 0; /* ENOSPC */
	}
	assert(*block_ptr == 0); /* Must be uninitialized */
	initialize_block(new_block, i, true);
	
	*block_ptr = new_block;
	i->i_mtime = curr_time;
	
	return new_block;
}

/* Finds the direct child of a parent inode. */
uint find_direct_child(uint parent, char *file) {
	inode *in;
	if (parent == 0 
			|| (in = get_valid_inode(parent)) == NULL 
			|| !IS(in->i_mode, EXT2_S_IFDIR)) {
		return 0;
	}
	/* Referring to root? */
	if (file == NULL || strlen(file) == 0 || strcmp(file, DELIMITER) == 0) {
		return parent;
	}
	return perform_on_children(in, NULL, file, NULL);
}

/* Gets the parent inode of the absolute path provided. 
 * If some intermediate path does not exist, return NULL.
 * If except is found in the path, return NULL.
 */
uint get_parent_inode_at_path_except(char *path, uint except) {
	char *token;
	uint curr, parent, temp, temp_parent;
	inode *out;
	char copy[strlen(path)];
	char *path_copy;
	
	strcpy(copy, path); /* strtok changes input... */
	
	token = strtok(copy, DELIMITER);
	curr = EXT2_ROOT_INO;
	parent = curr;
	
	while (token != NULL) {
		/* Dereference what is at "curr" */
		if (curr == except || curr == 0
				|| (out = get_valid_inode(curr)) == NULL
				|| (!IS(out->i_mode, EXT2_S_IFDIR)
					&& !IS(out->i_mode, EXT2_S_IFLNK))) {
			return 0;
		}
		temp_parent = curr;
		if (IS(out->i_mode, EXT2_S_IFLNK)) {
			/* Dereference the path, don't allow infinite loop */
			if (except != 0) { /* We already are checking a link */
				return 0; /* Just in case */
			}
			path_copy = read_file_contents(curr);
			temp_parent = get_inode_at_path_except(
						path_copy, curr);
			free(path_copy);
			
			if (temp_parent == 0 
				|| (out = get_valid_inode(temp_parent)) == NULL
				|| !IS(out->i_mode, EXT2_S_IFDIR)) {
				/* The symlink itself is wrong */
				return 0;
			}
			curr = temp_parent;
		}
		temp = find_direct_child(temp_parent, token);
		if (temp != curr) {
			parent = curr;
			curr = temp;
		}
		token = strtok(NULL, DELIMITER);
	}
	return parent;
}


/* Gets the parent inode of the absolute path provided. 
 * If some intermediate path does not exist, return 0
 */
uint get_parent_inode_at_path(char *path) {
	return get_parent_inode_at_path_except(path, 0);
}

/* Gets the inode at the absolute path provided. 
 * If some intermediate path does not exist, return 0
 */
uint get_inode_name_at_path_except(char *path, char *last_token, uint except) {
	return find_direct_child(get_parent_inode_at_path_except(path, except), 
								last_token);
}

/* Gets the inode at the absolute path provided. 
 * If some intermediate path does not exist, return 0
 */
uint get_inode_at_path_except(char *path, uint except) {
	uint parent = get_parent_inode_at_path_except(path, except);					
	char *last_token = find_last_token(path);
	return find_direct_child(parent, last_token);
}

/* Gets the inode at the absolute path provided. 
 * If some intermediate path does not exist, return 0
 */
uint get_inode_name_at_path(char *path, char *last_token) {
	return get_inode_name_at_path_except(path, last_token, 0);
}

/* Gets the inode at the absolute path provided. 
 * If some intermediate path does not exist, return 0
 */
uint get_inode_at_path(char *path) {
	return get_inode_at_path_except(path, 0);
}

/* Adds a directory entry to an inode.
 * Return true on success, false if no space left.
 */
bool add_dir_entry(inode *p, uint index, ubyte file_type, char *name) {
	uint str_len = strlen(name);
	uint *block, block_index;
	dir_entry *d, *new_d;
	uint dir_size, other_dir_size;
	ushort old_location;
	ubyte len;
	ubyte *block_ptr;
	inode *v = get_valid_inode(index);
	
	/* Must be a directory */
	assert(is_inode_valid(p) && v != NULL && IS(p->i_mode, EXT2_S_IFDIR));
	
	/* Truncate to EXT2_NAME_LEN */
	if (str_len > EXT2_NAME_LEN) {
		len = EXT2_NAME_LEN;
	} else {
		len = (ubyte)str_len;
	}
	dir_size = len + EXT2_DIR_DEFAULT_SIZE;
	/* Check if we have space */
	block = get_free_block_ptr(p, dir_size, has_free_dir_entry);
	if (block == NULL) {
		return false; /* ENOSPC */
	}
	block_index = *block;
	if (block_index == 0 || !get_block_bitmap(block_index)) { 
		/* Uninitialized block */
		block_index = add_new_block_to_inode(p);
		if (block_index == 0) { /* No room, ENOSPC */
			return false;
		}
	}
	block_ptr = get_valid_block(block_index);
	
	d = get_free_dir_entry(block_index, dir_size);
	
	if (d->inode == 0) { /* Uninitialized, we just use this one */
		new_d = d;
		/* Go to end of block */
		new_d->rec_len = (EXT2_BLOCK_SIZE - ((ubyte *)new_d - block_ptr));
	} else { /* Set rec_len to appropriate value, align to 4 bytes */
		other_dir_size = d->name_len + EXT2_DIR_DEFAULT_SIZE;
		old_location = d->rec_len; /* Save where it pointed */
		d->rec_len = other_dir_size + (EXT2_ALIGN - (other_dir_size % EXT2_ALIGN));
		new_d = (dir_entry *)((ubyte *)d + d->rec_len);
		/* Use the next one */
		new_d->rec_len = old_location - d->rec_len;
	}
	
	new_d->inode = index;
	new_d->file_type = file_type;
	new_d->name_len = len;
	strncpy(new_d->name, name, new_d->name_len);
	
	/* New link to this inode */
	v->i_links_count++;
	v->i_mtime = curr_time;
	return true;
}

/* Finds and initializes a new inode. Return NULL if not enough space. */
inode *new_inode(uint parent, uint num_blocks, ushort mode, char *name) {
	uint index;
	inode *i;
	inode *p = get_valid_inode(parent);
	
	/* If directory, needs a block */
	assert(num_blocks > 0 || !IS(mode, EXT2_S_IFDIR));
	
	if (p == NULL || !IS(p->i_mode, EXT2_S_IFDIR) || !has_space(1, num_blocks)) {
		return NULL;
	}
	
	index = find_free_inode();
	assert(index != 0);
	
	initialize_inode(index, true);
	
	i = get_valid_inode(index);
	assert (i != NULL);
	
	/* Add to parent */
	if (!add_dir_entry(p, index, to_dir_type(mode), name)) {
		return NULL;
	}
	i->i_mode = mode;
	if (IS(mode, EXT2_S_IFDIR)) {
		/* Add . and .. */
		if (!add_dir_entry(i, index, EXT2_FT_DIR, ".") 
				|| !add_dir_entry(i, parent, EXT2_FT_DIR, "..")) {
			return NULL;
		}
		/* One more used directory count */
		gd->bg_used_dirs_count++;
	}
	
	
	return i;
}

/* Writes a buffer into an inode.
 * Assumes that num_blocks free blocks are needed and checked for already.
 */
void write_file_data(inode *i, uint num_blocks, uint len, char *data) {
	uint index, written;
	ubyte *ptr;
	
	/* Must be a file */
	assert(!IS(i->i_mode, EXT2_S_IFDIR));
	
	if (len > 0) {
		if (num_blocks == 0) {
			/* Store into blocks */
			assert(IS(i->i_mode, EXT2_S_IFLNK) && len < EXT2_MIN_BLOCK_DATA);
			
			memcpy((char *)(i->i_block), data, len);
		} else {
			while (len > 0 && num_blocks > 0) {
				index = add_new_block_to_inode(i);
				ptr = get_valid_block(index);
				
				if (len > EXT2_BLOCK_SIZE) {
					written = EXT2_BLOCK_SIZE;
				} else {
					written = len;
				}
				memcpy((char *)ptr, data, written);
				len -= written;
				data += written;
				num_blocks--;
			}
		}
	}
	i->i_size = len;
}

/* Removes a directory entry from an inode. */
bool remove_dir_entry(inode *parent, char *name) {
	dir_entry *prev, *ret;
	uint block = get_block_with_entry(parent, name);
	
	if (block == 0) {
		return false; /* No directory entry exists */
	}
	/* block is a direct block index */
	/* Get the directory entries before and this one too */
	prev = search_inner_block(parent, block, NULL, name, 0, NULL, true);
	ret = search_inner_block(parent, block, NULL, name, 0, NULL, false);
	if (prev != NULL) { 
		/* Skip over ret entirely */
		prev->rec_len += ret->rec_len;
	}
	/* Zero out the entries */
	ret->inode = 0;
	memset(ret->name, 0, ret->name_len);
	ret->name_len = 0;
	ret->file_type = 0;
	return true;
}

/* Removes a child from a parent inode. */
void remove_child(dir_entry *entry, inode *parent) {
	char *dup = strndup(entry->name, entry->name_len);
	if (!is_special_dir(entry)) {
		remove_entry(entry->inode, dup, parent);
	} else {
		remove_dir_entry(parent, dup);
	}
	free(dup);
}

/* Removes an entry from parent. */
bool remove_entry(uint curr, char *name, inode *p) {
	inode *s = get_valid_inode(curr);
	
	assert(s != NULL && p != NULL);
	
	/* If directory, recurse */
	if (IS(s->i_mode, EXT2_S_IFDIR)) {
		perform_on_children(s, NULL, NULL, remove_child);
	}
	
	
	if (!remove_dir_entry(p, name)) {
		return false;
	}
	
	s->i_links_count--;
	
	if (IS(s->i_mode, EXT2_S_IFLNK) || IS(s->i_mode, EXT2_S_IFDIR) 
				|| s->i_links_count == 0) {
		/* Must remove inode */
		initialize_inode(curr, false);
	}
	
	return true;
}

/* Prints the name of a directory entry. */
void print_dir_entry(dir_entry *entry, inode *parent) {
	if (entry->inode > 0 && get_valid_inode(entry->inode) != NULL
				&& parent != NULL) {
		printf("%.*s\n", entry->name_len, entry->name);
	}
}

/* Prints the name of directory entries except .. and . */
void print_dir_entry_except(dir_entry *entry, inode *parent) {
	if (!is_special_dir(entry)) {
		print_dir_entry(entry, parent);
	}
}

/* Performs listing on a directory.
 * If "all" is true, also list the "." and "..".
 */
void print_dir_contents(uint curr, char *name, bool all) {
	inode *in = get_valid_inode(curr);
	
	if (in == NULL) {
		return;
	}
	if (!IS(in->i_mode, EXT2_S_IFDIR)) {
		printf("%s\n", name);
	} else if (all) {
		perform_on_children(in, NULL, NULL, print_dir_entry);
	} else {
		perform_on_children(in, NULL, NULL, print_dir_entry_except);
	}
}

/* Frees any memory associated with the memory mapping.
 * Return true on success.
 */
bool unload_disk(bool changed) {
	assert(disk != NULL);
	if (changed) {
		sb->s_wtime = curr_time;
	}
    if (munmap(disk, EXT2_SIMPLE * EXT2_BLOCK_SIZE) < 0) {
		perror("munmap");
		return false;
    }
    if (close(fd) < 0) {
		perror("close");
		return false;
    }
	disk = NULL;
	fd = -1;
	return true;
}