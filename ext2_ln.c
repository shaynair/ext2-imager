#include "ext2_imager.h"

/* Links a file on the EXT2 disk.
 *	If source file does not exist, return ENOENT.
 *  If link already exists as a file or link, return EEXIST.
 *  If source or link already exists and is a directory, return EISDIR.
 * Argument: If -s is provided, create a symlink instead.
 */
int main (int argc, char **argv) {
	uint curr, parent, src;
	char *last_token = NULL;
	char *spath, *tpath;
	ushort mode;
	inode *i;
	bool dir;
	uint len, num_blocks;
	bool sym = (argc == 5 && strcmp(argv[2], "-s") == 0);
	
	/* Check arguments */
	if (argc != 4 && !sym) {
		/* Wrong usage */
		fprintf(stderr, "Incorrect parameters. Usage: ./ext2_ln <image> [-s] \
<source file, absolute path on EXT2> <target file, absolute path on EXT2>\n");
		return EXIT_FAILURE;
	}
	if (!load_simple_disk(argv[1])) {
		fprintf(stderr, "Failed to load the disk.\n");
		return EXIT_FAILURE;
	}
	
	spath = argv[argc - 2];
	tpath = argv[argc - 1];
	if (strlen(spath) == 0 || spath[0] != '/' || spath[strlen(spath) - 1] == '/') {
		fprintf(stderr, "Source path must be absolute (so must start with /) \
and must refer to a file (so cannot end with /)\n");
		unload_disk(false);
		return EINVAL;
	}
	if (strlen(tpath) == 0 || tpath[0] != '/' || tpath[strlen(tpath) - 1] == '/') {
		fprintf(stderr, "Target path must be absolute (so must start with /) \
and must refer to a file (so cannot end with /)\n");
		unload_disk(false);
		return EINVAL;
	}
	
	src = get_inode_at_path(spath);
	parent = get_parent_inode_at_path(tpath);
	last_token = find_last_token(tpath);
	if (src == 0 || parent == 0) {
		/* Intermediate path does not exist */
		fprintf(stderr, "Path does not exist\n");
		unload_disk(false);
		return ENOENT;
	}
	
	if ((curr = find_direct_child(parent, last_token)) != 0) {
		/* Exists */
		dir = IS(get_valid_inode(curr)->i_mode, EXT2_S_IFDIR);
		fprintf(stderr, "Target path exists already\n");
		unload_disk(false);
		return (dir ? EISDIR : EEXIST);
	}
	
	mode = get_valid_inode(src)->i_mode;
	if (IS(mode, EXT2_S_IFDIR)) {
		/* Source is directory */
		fprintf(stderr, "Source path is a directory\n");
		unload_disk(false);
		return EISDIR;
	}
	if (strlen(last_token) > EXT2_NAME_LEN) {
		/* Too long */
		fprintf(stderr, "Name %s is too long\n", last_token);
		unload_disk(false);
		return ENAMETOOLONG;
	}
	if (sym) {
		/* Symbolic link - new inode with path in blocks */
		len = strlen(spath);
		if (len >= EXT2_MIN_BLOCK_DATA) {
			num_blocks = DIV_UP(len, EXT2_BLOCK_SIZE);
		} else {
			num_blocks = 0;
		}
		if ((i = new_inode(parent, num_blocks, EXT2_S_IFLNK, last_token)) == NULL) {
			/* No space */
			fprintf(stderr, "No space found on disk\n");
			unload_disk(false);
			return ENOSPC;
		}
		write_file_data(i, num_blocks, len, spath);
	} else {
		/* Write a regular file with same inode and mode as src but different name */
		add_dir_entry(get_valid_inode(parent), src, mode, last_token);
	}
	
	if (!unload_disk(true)) {
		fprintf(stderr, "Failed to unload the disk.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}