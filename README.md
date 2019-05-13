# Virtual File System Simulation
This lab involved testing various attributes of virtual file system images in the FAT16 format. Each aspect of a FAT16 file system is convered in a different test parameter when running the program.

## Parameters
The program takes in the required argument `--image`, which takes in an image file and loads it into memory with `mmap()`.

The `--test-mmap` optional argument reads in a size and location from stdin and prints out the byte at that location in a specific size.

The `--test-boot-sector` optional argument prints out image information based on FAT16 boot sector headers.

The `--test-directory-entry` optional argument prints out the specified root directory entry information.

The `--test-file-clusters` optional argument prints out the linked list of clusters associated with the specified file entry.

The `--test-file-name` optional argument searches the image for a specified filename and prints out the file entry information.

The `--test-file-contents` optional argument searches the image for a specifed filename and prints out the file contents.

The `--test-num-entries` optional argument searches the file system for the number of files and directories.

The `--test-space-usage` optional argument calculates the space usage statistics for the given file system.

The `--test-largest-file` optional argument searches the file system for the largest file and the path to the file.

The `--test-cookie` optional argument searches all of the files in the file system for a particular string.

The `--test-num-dir-levels` optional argument searches the file system for the deepest level of subdirectories.

The `--test-oldest-file` optional argument searches the file system for the oldest file.

The `--output-fs-data` optional argument prints out the results of the previous 6 parameters.

The `--write-fs-data` optional argument is supposed to write out the contents of `--output-fs-data` to a file and place it in the given image. Currently, the program with this argument does not actually do anything.

## Build
To build the `fs` executable:
```
$ gcc -o fs fs.c
```

## Run
```
$ ./fs --image <image> --<optional test parameter>
```

## Test
```
$ runtest --tool=fs <optional specified test files>
```
Currently, there are 1013 passes out of 1052 tests because some test parameters are not implemented yet.

It takes about 7 minutes to run the entire test suite.

## Notes
Only one optional argument may be used at a time.
