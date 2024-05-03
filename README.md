## Build Executable

call 'make'

if you wish to clean the output: call make clean

## Commands

### Info:

Displays key drive information

./reader 3430-good.img info

### List:

Prints drive contents as a tree structure

./reader 3430-good.img list

### Get:

Retrieves a specified file and writes it into the output folder

./reader 3430-good.img get BOOKS/WARAND~1.TXT
