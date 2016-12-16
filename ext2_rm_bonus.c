#include "ext2_imager.h"

/* Removes a file or link, or recursively removes a directory on the EXT2 disk.
 *	If file does not exist, return ENOENT.
 *  If file is a directory, return EISDIR.
 * Argument: If -r is provided, remove a directory instead.
 *			If a file or link is provided, ignore the -r.
 */
int main (int argc, char **argv) {
	bool dir = (argc == 4 && strcmp(argv[2], "-r") == 0);
	char *path = argv[argc - 1], *last_token;
	uint curr, parent;
	
	/* Check arguments */
	if (argc != 3 && !dir) {
		/* Wrong usage */
		fprintf(stderr, "Incorrect parameters. Usage: ./ext2_rm_bonus \
<image> [-r] <absolute path on EXT2>\n");
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
	curr = find_direct_child(parent, last_token);
	if (curr == 0) {
		/* Intermediate path does not exist */
		fprintf(stderr, "Path does not exist\n");
		unload_disk(false);
		return ENOENT;
	}
	if (curr == parent || curr == EXT2_ROOT_INO) {
		/* Trying to delete root */
		fprintf(stderr, "Cannot delete special directory\n");
		unload_disk(false);
		return EINVAL;
	}
	if (!dir) {
		if (path[strlen(path) - 1] == '/') {
			fprintf(stderr, "Path must refer to a file (so cannot end with /)\n");
			unload_disk(false);
			return EINVAL;
		}
		if (IS(get_valid_inode(curr)->i_mode, EXT2_S_IFDIR)) {
			/* Source is directory */
			fprintf(stderr, "Path is a directory\n");
			unload_disk(false);
			return EISDIR;
		}
	}
	
	if (!remove_entry(curr, last_token, get_valid_inode(parent))) {
		/* Source is directory */
		fprintf(stderr, "Unknown error...\n");
		unload_disk(false);
		return EINVAL;
	}
	
	if (!unload_disk(true)) {
		fprintf(stderr, "Failed to unload the disk.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}