#include "ext2_imager.h"

/* Prints all files and directories in a given absolute path in the EXT2 disk. 
 * Argument: If -a is specified, print . and .. as well.
 *	If the path does not exist, return ENOENT and print "No such file or directory".
 *	If the path is a file or link, simply print the file name (without . or ..)
 */
int main (int argc, char **argv) {
	uint curr, parent;
	char *last_token = NULL;
	char *path = argv[argc - 1];
	bool all = (argc == 4 && strcmp(argv[2], "-a") == 0);
	bool mustBeDir;
	
	/* Check arguments */
	if (argc != 3 && !all) {
		/* Wrong usage */
		fprintf(stderr, "Incorrect parameters. Usage: ./ext2_ls <image> \
[-a] <absolute path on EXT2>\n");
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
	mustBeDir = path[strlen(path) - 1] == '/';
	parent = get_parent_inode_at_path(path);					
	last_token = find_last_token(path);
	curr = find_direct_child(parent,  last_token);
	if (curr == 0) { 
		fprintf(stderr, "No such file or directory\n");
		unload_disk(false);
		return ENOENT;
	}
	if (!IS(get_valid_inode(curr)->i_mode, EXT2_S_IFDIR) && mustBeDir) {
		/* Should be a directory, but isn't */
		fprintf(stderr, "Path refers to a file or link, but ends in /, which is invalid\n");
		unload_disk(false);
		return ENOENT;
	}
	print_dir_contents(curr, last_token, all);

	if (!unload_disk(false)) {
		fprintf(stderr, "Failed to unload the disk.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}