# ext2-imager

> A C implementation of common Linux file system commands that can be used on EXT2 hard disks.


## How to use

1. Run `make` in the root directory.

2. Extract `sample_input.zip`

3. Run the desired commands.

## How to run

In all commands, `<image>` is the file path of the EXT2 image file.

```
# Copies a file from the native OS to the EXT2 image.
./ext2_cp <image> 
<path on native OS> <absolute path on EXT2>

# Creates hard or soft links on the EXT2 image.
./ext2_ln <image> [-s] 
<source file, absolute path on EXT2> <target file, absolute path on EXT2>

# Lists files on the EXT2 image.
./ext2_ls <image> [-a] <absolute path on EXT2>

# Creates a directory on the EXT2 image.
./ext2_mkdir <image> 
<absolute path on EXT2>

# Removes a directory or file on the EXT2 image.
./ext2_rm_bonus 
<image> [-r] <absolute path on EXT2>
```
