#include "ext2_imager.h"

/* Copies a file from the native OS onto a location on the EXT2 disk.
 *	If either path does not exist, return ENOENT.
 */
int main (int argc, char **argv) {
	char *path, *spath, *last_token;
	ubyte *file;
	uint parent;
	inode *i;
	uint len, num_blocks;
	int fd;
	struct stat st;
	
	/* Check arguments */
	if (argc != 4) {
		/* Wrong usage */
		fprintf(stderr, "Incorrect parameters. Usage: ./ext2_cp <image> \
<path on native OS> <absolute path on EXT2>\n");
		return EXIT_FAILURE;
	}
	if (!load_simple_disk(argv[1])) {
		fprintf(stderr, "Failed to load the disk.\n");
		return EXIT_FAILURE;
	}
	/* Load source file */
	spath = argv[argc - 2];
	if (stat(spath, &st) != 0 || !S_ISREG(st.st_mode)) {
		/* Error, or not a regular file */
		fprintf(stderr, "No such source file or directory %s\n", spath);
		return EINVAL;
	}
	/* Check target file */
	path = argv[argc - 1];
	if (strlen(path) == 0 || path[0] != '/') {
		fprintf(stderr, "Target path must be absolute (so must start with /)\n");
		unload_disk(false);
		return EINVAL;
	}
	
	parent = get_inode_at_path(path);
	last_token = find_last_token(spath);
	if (parent == 0 || !IS(get_valid_inode(parent)->i_mode, EXT2_S_IFDIR)) {
		/* Intermediate path does not exist or is not directory */
		fprintf(stderr, "Invalid directory path\n");
		unload_disk(false);
		return ENOENT;
	}
	
	if (find_direct_child(parent, last_token) != 0) {
		/* Exists */
		fprintf(stderr, "File %s already exists\n", last_token);
		unload_disk(false);
		return EEXIST;
	}
	
	if (strlen(last_token) > EXT2_NAME_LEN) {
		/* Too long */
		fprintf(stderr, "Name %s is too long\n", last_token);
		unload_disk(false);
		return ENAMETOOLONG;
	}
	
	/* Now get source */
	if ((fd = open(spath, O_RDONLY)) < 0) {
		perror("open");
		return EXIT_FAILURE;
	}
	
	len = lseek(fd, 0, SEEK_END);
	if ((file = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		return EXIT_FAILURE;
	}

	/* Write data */
	num_blocks = DIV_UP(len, EXT2_BLOCK_SIZE);
	if ((i = new_inode(parent, num_blocks, EXT2_S_IFREG, last_token)) == NULL) {
		/* No space */
		fprintf(stderr, "No space found on disk\n");
		
		munmap(file, len);
		close(fd);
		unload_disk(false);
		return ENOSPC;
	}
	write_file_data(i, num_blocks, len, (char *)file);
	
	/* Cleanup */
    if (munmap(file, len) < 0) {
		perror("munmap");
		return EXIT_FAILURE;
    }
    if (close(fd) < 0) {
		perror("close");
		return EXIT_FAILURE;
    }
	if (!unload_disk(true)) {
		fprintf(stderr, "Failed to unload the disk.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}