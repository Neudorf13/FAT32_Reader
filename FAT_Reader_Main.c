/*
 * Description:
 * FAT_Reader_Main.c is a program designed to read in a FAT32 filesystem and perform various operations on the filesystem. It prints
 * key information about the drive by running the info command. It performs necessary tasks such as first verify the
 * drive has a valid FAT signature. If you run the list command it prints a tree structure view of the files and directories
 * that are currently on the drive. By calling the get command, it will retrieve the requested file and write it to the
 * output folder.
 */

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>   // for open
#include <unistd.h>
#include "fat32.h"

//globals
int fd;
int fatSectionStart;
int dataSectionStart;

//bootSector
uint32_t rootDirectoryClusterNum;
uint16_t bytesPerSector;
uint16_t reservedSectors;
uint32_t sectorsPerFAT;
uint8_t numFATs;
uint16_t numbReservedSectors;
uint32_t totalSectors;
uint8_t sectorsPerCluster;
uint16_t fsInfoSector;
int bytesPerCluster;

//verify
uint8_t fatMedia;

fat32BS bootSector;
struct FSInfo fsInfo;
struct LongFileName longFileStruct;

//verifies the first 2 indexes of the FAT are valid
int verifyFAT() {
    int result = 0;
    uint32_t fat0, fat1;
    lseek(fd, fatSectionStart, SEEK_SET);
    read(fd, &fat0, sizeof(uint32_t));

    lseek(fd, fatSectionStart+4, SEEK_SET);
    read(fd, &fat1, sizeof(uint32_t));

    uint8_t lowerByte = fat0 & 0xFF;

    if(lowerByte == fatMedia && fat1 == 0x0FFFFFFF) {
        result = 1;
    }
    return result;
}

//prints key information about the drive
void realInfoCommand() {

    //1. Drive name (both OEM name and Volume label)
    char driveName[BS_OEMName_LENGTH + 1];
    strncpy(driveName, bootSector.BS_OEMName, BS_OEMName_LENGTH);
    driveName[BS_OEMName_LENGTH] = '\0';
    char volumeLabel[BS_VolLab_LENGTH + 1];
    strncpy(volumeLabel, bootSector.BS_VolLab, BS_VolLab_LENGTH);
    volumeLabel[BS_VolLab_LENGTH] = '\0';


    //1.
    printf("Drive name: %s\n", volumeLabel);
    printf("OEM name: %s\n", driveName);

    //2.
    int fsInfoOffSet = (int)fsInfoSector * bytesPerSector;
    lseek(fd, fsInfoOffSet, SEEK_SET);
    read(fd, &fsInfo, sizeof(struct FSInfo));


    uint32_t freeClusters = fsInfo.free_count;
    uint32_t freeSpace = freeClusters * (uint32_t) sectorsPerCluster * (uint32_t) bytesPerSector;
    printf("free space: %u KB\n", freeSpace/1024);

    //3.
    uint32_t totalSpace = totalSectors * (uint32_t)bytesPerSector;
    uint32_t reservedSpace = numbReservedSectors * (uint32_t)bytesPerSector;
    uint32_t fatSpace = (uint32_t)numFATs * sectorsPerFAT * (uint32_t)bytesPerSector;
    uint32_t unUsableSpace = reservedSpace + fatSpace;
    uint32_t totalUsableSpace = totalSpace - unUsableSpace;
    printf("Total space: %u KB\n", totalSpace/1024);
    printf("Total usable space: %u KB\n", totalUsableSpace/1024);


    //4.
    printf("Cluster size in sectors: %u\n", sectorsPerCluster);
    printf("Cluster size is: %d bytes\n", bytesPerCluster);

}

//used to indent the printing of the file structure in order to give a tree appearance
void printLevel(int level) {

    char *line = malloc(level + 1); // Allocate memory for the string (+1 for null terminator)
    if (line == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    memset(line, '-', level); // Fill the string with '-' characters
    line[level] = '\0'; // Null-terminate the string

    printf("%s", line); // Print the string

    free(line); // Free the allocated memory
}

//looks at the fat table and returns the next cluster that the inputted cluster is pointing to
uint32_t getNextCluster(int clusterIndex) {
    uint32_t nextCluster;
    int currIndex = fatSectionStart + (clusterIndex * 4);

    lseek(fd, currIndex, SEEK_SET);
    read(fd, &nextCluster, sizeof(uint32_t));

    return nextCluster;
}

//handles the high and low bits to find out which cluster the directory begins storing its data in
uint32_t calcHighLow(uint16_t low, uint16_t high) {
    uint32_t high2;
    uint32_t low2;
    uint32_t result;

    high2 = (uint32_t)high;
    low2 = (uint32_t)low;

    high2 = high2 << 16;    //shift left by 16bits

    result = high2 | low2;  //OR the two values

    return result;
}

//handles the case where the filename is a long name -> prints the longname
void longName(int longNameOffSet) {

    char buffer[100];
    int i;
    int count = 0;
    int done = 0;
    int currOffSet = longNameOffSet;
    while(!done) {
        lseek(fd, currOffSet, SEEK_SET);
        read(fd, &longFileStruct, sizeof(struct LongFileName));

        uint16_t* name1 = longFileStruct.LDIR_Name1;
        uint16_t* name2 = longFileStruct.LDIR_Name2;
        uint16_t* name3 = longFileStruct.LDIR_Name3;


        i = 0;
        while(i < 5) {
            buffer[count] = (char)name1[i];
            i++;
            count++;
        }

        i = 0;
        while(i < 6) {
            buffer[count] = (char)name2[i];
            i++;
            count++;
        }

        i = 0;
        while(i < 2) {
            buffer[count] = (char)name3[i];
            i++;
            count++;
        }

        if(longFileStruct.LDIR_Ord > 0x40) {
            done = 1;
        }

        currOffSet -= 32;
    }

    buffer[count] = '\0';

    char *str = (char *)malloc(strlen(buffer) + 1);
    strcpy(str, buffer);
    printf("%s\n", str);
    free(str);
}

//readDirectory recursively travels through the file system, for each directory it looks through each sector attempting to find valid files
//for found valid files, it prints them out
//for found subdirectories, recursively calls to look through the subdirectory
void readDirectory(int clusterNum, int level) {

    struct DirInfo currCluster;
    int currDataEntryIndex = dataSectionStart;
    currDataEntryIndex = currDataEntryIndex + ((clusterNum-2) * 512);
    uint32_t resultHighLow;
    int longNameOffSet;

    for(int i = 0; i < 16; i++) {
        lseek(fd, currDataEntryIndex, SEEK_SET);
        read(fd, &currCluster, sizeof(struct DirInfo));

        if((uint8_t)currCluster.dir_name[0] != 0xE5) {

            if(currCluster.dir_attr == 32) {
                printLevel(level+1);

                if(strchr(currCluster.dir_name, '~') != NULL) {

                    longNameOffSet = currDataEntryIndex - 32;
                    longName(longNameOffSet);

                }
                else {
                    //char* firstEight = currCluster.dir_name;
                    char firstEight[9];
                    strncpy(firstEight, currCluster.dir_name, 8);
                    firstEight[8] = '\0';
                    int countSpaces = 0;
                    for(int j = 0; j < 8; j++) {
                        if(firstEight[j] == ' ') {
                            countSpaces++;
                        }
                    }
                    char *concatenated_str = (char *)malloc(13-countSpaces);

                    memcpy(concatenated_str, firstEight, 8-countSpaces);
                    strcat(concatenated_str, ".");
                    strncat(concatenated_str, currCluster.dir_name + 8, 3);
                    printf("%s\n", concatenated_str);

                    free(concatenated_str);
                }
            }

            else if(currCluster.dir_attr == 16) {   //folder
                if(level == 0 || i > 1) {
                    printLevel(level+1);
                    printf("%.11s\n", currCluster.dir_name);
                    resultHighLow = calcHighLow(currCluster.dir_first_cluster_lo, currCluster.dir_first_cluster_hi);

                    readDirectory((int)resultHighLow, level+1);
                }

            }
        }

        currDataEntryIndex += 32;
    }


    //look up cluster number in FAT
    uint32_t nextCluster = getNextCluster(clusterNum);
    if(nextCluster < 0x0FFFFFF8) {
        readDirectory((int)nextCluster, level);
    }

}

//drive function to begin the list command as readDirectory is recursive
void listCommand() {
    printf("root\n");
    readDirectory((int)rootDirectoryClusterNum, 0);
}

//removes white spaces from inputted string
char* stripWhiteSpace(const char* string) {
    int i = 0;
    char newString[12];
    while(i < 11 && string[i] != ' ') {
        newString[i] = string[i];
        i++;
    }
    newString[i] = '\0';

    char *str = (char *)malloc(strlen(newString) + 1);
    strcpy(str, newString);

    return str;
}

//used to determine how many levels there are for a given path name
int countSlashes(char* path) {
    int count = 0;
    while (*path != '\0') {
        if (*path == '/') {
            count++;
        }
        path++;
    }
    return count;
}

//adds the period between filename and file type
char* includePeriod(char* str) {
    char* firstEight = str;
    int countSpaces = 0;
    for(int j = 0; j < 8; j++) {
        if(firstEight[j] == ' ') {
            countSpaces++;
        }
    }

    char *concatenated_str = (char *)malloc(13-countSpaces);

    strncpy(concatenated_str, str, 8-countSpaces);
    strcat(concatenated_str, ".");
    strncat(concatenated_str, str + 8, 3);

    return concatenated_str;
}

//fetches a file passed on the inputted path, informs user if there is no such file in given path
//works by first search through each level of the path until it finds the appropriate directory/filename for the given level
//if the function finds all the path items including the final file at the end of the path, will begin writing the file to output
void fetchFile(char* path) {
    printf("Begin fetch of: %s\n", path);

    char* newPath = (char*)malloc(strlen(path) + 1);
    strcpy(newPath, path);

    const char *destinationFileName;
    int destinationFile;
    int fileSize;
    int amount;
    char *buffer;

    int size = countSlashes(path) + 1;
    char* parts[size];
    int m = 0;
    char * token = strtok(path, "/");
    while(m < size) {
        parts[m] = token;
        token = strtok(NULL, "/");
        m++;
    }

    destinationFileName = parts[size-1];
    printf("%s\n", destinationFileName);
    char *outputFileName = malloc(strlen("Output/") + strlen(destinationFileName) + 1);
    strcpy(outputFileName, "Output/");
    strcat(outputFileName, destinationFileName);
    printf("%s\n", outputFileName);

    struct DirInfo currCluster;
    uint32_t resultHighLow;

    int clusterNum = 2;
    int currOffSet = dataSectionStart + ((clusterNum-2) * bytesPerCluster); //init to root directory
    char* newString;
    int found;

    for(int i = 0; i < size; i++) {
        found = 0;
        for(int j = 0; j < 16; j++) {
            lseek(fd, currOffSet, SEEK_SET);
            read(fd, &currCluster, sizeof(struct DirInfo));

            if((uint8_t)currCluster.dir_name[0] != 0xE5) {

                if(i == size-1) {
                    //newString = stripWhiteSpace(currCluster.dir_name);
                    //newString = includePeriod(newString);
                    newString = includePeriod(currCluster.dir_name);
                }
                else {
                    newString = stripWhiteSpace(currCluster.dir_name);
                }


                if(strcmp(parts[i], newString) == 0) {
                    found = 1;
                    free(newString);
                    resultHighLow = calcHighLow(currCluster.dir_first_cluster_lo, currCluster.dir_first_cluster_hi);
                    clusterNum = (int)resultHighLow;
                    break;
                }
                else {
                    free(newString);
                }
            }
            currOffSet+=32;
        }
        if(!found) {
            break;
        }
        else {
            currOffSet = dataSectionStart + ((clusterNum-2) * bytesPerCluster);
        }

    }

    if(found) {
        destinationFile = open(outputFileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        buffer = (char *)malloc(bytesPerCluster);
        currOffSet = dataSectionStart + ((clusterNum-2) * bytesPerCluster);
        fileSize = (int)currCluster.dir_file_size;

        while(clusterNum < 0x0FFFFFF8) {
            lseek(fd, currOffSet, SEEK_SET);
            if(fileSize >= bytesPerCluster) {
                amount = bytesPerCluster;
            }
            else {
                amount = fileSize;
            }
            read(fd, buffer, amount);
            write(destinationFile, buffer, amount);
            fileSize -= bytesPerCluster;
            clusterNum = (int)getNextCluster(clusterNum);
            currOffSet = dataSectionStart + ((clusterNum-2) * bytesPerCluster);
        }

        printf("Completed fetch of: %s\n", parts[size-1]);
    }
    else {
        printf("Could not find file from the path: %s\n", newPath);    //fix the path variable
    }

    free(newPath);
}

//initializes all necessary values from the boot sector as well as performs a few basic calculations that can be reused
void init() {
    read(fd, &bootSector, sizeof(fat32BS));
    rootDirectoryClusterNum =  bootSector.BPB_RootClus; //2
    bytesPerSector = bootSector.BPB_BytesPerSec;   //512
    reservedSectors = bootSector.BPB_RsvdSecCnt;   //32
    sectorsPerFAT = bootSector.BPB_FATSz32;    //536
    numFATs = bootSector.BPB_NumFATs;   //2
    numbReservedSectors = bootSector.BPB_RsvdSecCnt;   //32
    totalSectors = bootSector.BPB_TotSec32;    //69632
    sectorsPerCluster = bootSector.BPB_SecPerClus;  //1
    fsInfoSector = bootSector.BPB_FSInfo;
    fatMedia = bootSector.BPB_Media;

    fatSectionStart = (int) ((uint32_t)bytesPerSector * (uint32_t)reservedSectors);

    //16,384 + (536 * 512 bytes * 2) = 565,248 bytes
    dataSectionStart = (int)(fatSectionStart + (sectorsPerFAT * bytesPerSector * numFATs));

    bytesPerCluster = (int)((uint32_t)bytesPerSector * (uint32_t)sectorsPerCluster); //512 bytes
}



int main(int argc, char *argv[]) {
    //handle args
    if (argc < 3 || argc > 4) {
        printf("2 or 3 arguments are required.\n");
        return EXIT_FAILURE;
    }

    char *fileName = strdup(argv[1]);
    char *command = strdup(argv[2]);

    //open file descriptor
    fd = open(fileName, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    //initialize FAT values
    init();

    //verify FAT signatures
    if(verifyFAT() != 1) {
        printf("Invalid FAT Signatures.\n");
        return 1;
    }

    //perform appropriate task for appropriate command
    if(strcmp(command, "info") == 0) {
        printf("Info Command!\n");
        realInfoCommand();
    }
    else if(strcmp(command, "list") == 0) {
        printf("List Command!\n");
        listCommand();
    }
    else if(strcmp(command, "get") == 0 && argc == 4) {
        char *path = strdup(argv[3]);
        printf("Get Command!\n");
        fetchFile(path);
    }
    else {
        printf("%s is an Invalid Command!\n", command);
    }

    //closes file descriptor
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;

}
