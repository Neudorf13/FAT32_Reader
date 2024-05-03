#include <inttypes.h>
#ifndef FAT32_H
#define FAT32_H

#include <inttypes.h>

/* boot sector constants */
#define BS_OEMName_LENGTH 8
#define BS_VolLab_LENGTH 11
#define BS_FilSysType_LENGTH 8 

#pragma pack(push)
#pragma pack(1)
struct fat32BS_struct {
	char BS_jmpBoot[3];
	char BS_OEMName[BS_OEMName_LENGTH];
	uint16_t BPB_BytesPerSec;       //Bytes per sector -> ALWAYS 512 bytes
	uint8_t BPB_SecPerClus;         //Sectors per Cluster -> 1,2,4,8,16,32,64,128
	uint16_t BPB_RsvdSecCnt;        //Number of reserved Sectors -> usually 32
	uint8_t BPB_NumFATs;            //Number of FATS -> always 2
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATSz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;          //total number of sectors for the ENTIRE volume
	uint32_t BPB_FATSz32;           //Sectors per FAT
	uint16_t BPB_ExtFlags;
	uint8_t BPB_FSVerLow;
	uint8_t BPB_FSVerHigh;
	uint32_t BPB_RootClus;          //cluster # for first cluster of root directory
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	char BPB_reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	char BS_VolLab[BS_VolLab_LENGTH];
	char BS_FilSysType[BS_FilSysType_LENGTH];
	char BS_CodeReserved[420];
	uint8_t BS_SigA;
	uint8_t BS_SigB;
};
#pragma pack(pop)

typedef struct fat32BS_struct fat32BS;

#pragma pack(push)
#pragma pack(1)
struct FSInfo{
    uint32_t lead_sig;
    uint8_t reserved1[480];
    uint32_t signature;
    uint32_t free_count;
    uint32_t next_free;
    uint8_t reserved2[12];
    uint32_t trail_signature;
};

struct DirInfo {    //meant for cluster
    char dir_name[11];  //8-3 formatted: 8chars for name, 3 for file extension
    uint8_t dir_attr;   // various attributes of file/directory, ex. read-only, hidden, system, volume label, directory, or archive.
    uint8_t dir_ntres;
    uint8_t dir_crt_time_tenth;
    uint16_t dir_crt_time;
    uint16_t dir_crt_date;
    uint16_t dir_last_access_time;
    uint16_t dir_first_cluster_hi;
    uint16_t dir_wrt_time;
    uint16_t dir_wrt_date;
    uint16_t dir_first_cluster_lo;
    uint32_t dir_file_size;     //size of file in bytes
};

#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct Fsinfo {
    uint32_t FSI_LeadSig;
    char FSI_Reserved1[480];
    uint32_t FSI_StrucSig;
    uint32_t FSI_Free_Count;
    // that's all we need!
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct LongFileName {
    uint8_t LDIR_Ord;
    uint16_t LDIR_Name1[5]; //5 2-byte chars
    uint8_t LDIR_Attr;
    uint8_t LDIR_Type;
    unsigned char LDIR_Chksum;
    uint16_t LDIR_Name2[6]; //6 2-byte chars
    uint16_t LDIR_FstClusLO;
    uint16_t LDIR_Name3[2]; //2 2-byte chars
};
#pragma pack(pop)

#define EOC 0x0FFFFFFF  // page 18
#define BAD_CLUSTER 0x0FFFFFF7

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID

#endif
