#include "ext2_imager.h"

/* Makes a directory on the EXT2 disk.
 *	If any intermediate path does not exist, return ENOENT.
 *  If the directory already exists, return EEXIST.
 */
int main (int argc, char **argv) {
	uint parent;
	char *last_token = NULL;
	char *path = argv[argc - 1];
	
	/* Check arguments */
	if (argc != 3) {
		/* Wrong usage */
		fprintf(stderr, "Incorrect parameters. Usage: ./ext2_mkdir <image> \
<absolute path on EXT2>\n");
		return EXIT_FAILURE;
	}
	if (!load_simple_disk(argv[1])) {
		fprintf(stderr, "Failed to load the disk.\n");
		return EXIT_FAILURE;
	}
	if (strlen(path) == 0 || path[0] != '/') {
		fprintf(stderr, "Path must be absolute (so must start with /)\n");
		unload_disk(false);
		return EINVAL;
	}
	parent = get_parent_inode_at_path(path);
	last_token = find_last_token(path);
	if (parent == 0) {
		/* Intermediate path does not exist */
		fprintf(stderr, "No directory found\n");
		unload_disk(false);
		return ENOENT;
	}
	
	if (find_direct_child(parent, last_token) != 0) {
		/* Directory exists */
		fprintf(stderr, "%s exists already\n", last_token);
		unload_disk(false);
		return EEXIST;
	}
	if (strlen(last_token) > EXT2_NAME_LEN) {
		/* Too long */
		fprintf(stderr, "Name %s is too long\n", last_token);
		unload_disk(false);
		return ENAMETOOLONG;
	}
    if (new_inode(parent, 1, EXT2_S_IFDIR, last_token) == NULL) {
		/* No space */
		fprintf(stderr, "No space found on disk\n");
		unload_disk(false);
		return ENOSPC;
	}
	
	if (!unload_disk(true)) {
		fprintf(stderr, "Failed to unload the disk.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}