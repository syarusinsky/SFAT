#ifndef BOOT_SECTOR_HPP
#define BOOT_SECTOR_HPP

/**************************************************************************
 * The BootSector class defines a boot sector entry, as well as functions
 * for obtaining its values.
**************************************************************************/

#include <stdint.h>

#include "PartitionTable.hpp"

// boot sector offsets and sizes
#define BOOT_SEC_SIZE_IN_BYTES 				512
#define BOOT_SEC_JUMP_OFFSET 				0x000
#define BOOT_SEC_JUMP_SIZE 				3
#define BOOT_SEC_OEM_NAME_OFFSET 			0x003
#define BOOT_SEC_OEM_NAME_SIZE          		8
#define BOOT_SEC_SECTOR_SIZE_OFFSET 			0x00B
#define BOOT_SEC_SECTOR_SIZE_SIZE 			2
#define BOOT_SEC_NUM_SECS_PER_CLUSTER_OFFSET 		0x00D
#define BOOT_SEC_NUM_SECS_PER_CLUSTER_SIZE 		1
#define BOOT_SEC_RESERVED_SECS_OFFSET 			0x00E
#define BOOT_SEC_RESERVED_SECS_SIZE 			2
#define BOOT_SEC_NUM_FATS_OFFSET 			0x010
#define BOOT_SEC_NUM_FATS_SIZE 				1
#define BOOT_SEC_NUM_DIRS_IN_ROOT_OFFSET 		0x011
#define BOOT_SEC_NUM_DIRS_IN_ROOT_SIZE 			2
#define BOOT_SEC_NUM_SECS_ON_DISK_LT_32MB_OFFSET 	0x013
#define BOOT_SEC_NUM_SECS_ON_DISK_LT_32MB_SIZE 		2
#define BOOT_SEC_MEDIA_DESCRIPTOR_OFFSET 		0x015
#define BOOT_SEC_MEDIA_DESCRIPTOR_SIZE 			1
#define BOOT_SEC_NUM_SECS_PER_FAT_OFFSET 		0x016
#define BOOT_SEC_NUM_SECS_PER_FAT_SIZE 			2
#define BOOT_SEC_NUM_SECS_PER_TRACK_OFFSET 		0x018
#define BOOT_SEC_NUM_SECS_PER_TRACK_SIZE 		2
#define BOOT_SEC_NUM_HEADS_OFFSET 			0x01A
#define BOOT_SEC_NUM_HEADS_SIZE 			2
#define BOOT_SEC_NUM_HIDDEN_SECS_OFFSET 		0x01C
#define BOOT_SEC_NUM_HIDDEN_SECS_SIZE 			4
#define BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_OFFSET 	0x020
#define BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_SIZE 		4
#define BOOT_SEC_NUM_SECS_PER_FAT_FAT32_OFFSET 		0x024
#define BOOT_SEC_NUM_SECS_PER_FAT_FAT32_SIZE 		4
#define BOOT_SEC_FAT_MIRR_ACTIVE_FAT_FAT32_OFFSET 	0x028
#define BOOT_SEC_FAT_MIRR_ACTIVE_FAT_FAT32_SIZE 	2
#define BOOT_SEC_FILE_SYSTEM_VER_FAT32_OFFSET 		0x02A
#define BOOT_SEC_FILE_SYSTEM_VER_FAT32_SIZE 		2
#define BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_OFFSET 	0x02C
#define BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_SIZE 	4
#define BOOT_SEC_FSINFO_SEC_NUM_FAT32_OFFSET 		0x030
#define BOOT_SEC_FSINFO_SEC_NUM_FAT32_SIZE 		2
#define BOOT_SEC_BACKUP_SEC_NUM_FAT32_OFFSET 		0x032
#define BOOT_SEC_BACKUP_SEC_NUM_FAT32_SIZE 		2
#define BOOT_SEC_RESERVED_FAT32_OFFSET 			0x034
#define BOOT_SEC_RESERVED_FAT32_SIZE 			12
#define BOOT_SEC_DRIVE_NUMBER_FAT32_OFFSET 		0x040
#define BOOT_SEC_DRIVE_NUMBER_FAT16_OFFSET 		0x024
#define BOOT_SEC_DRIVE_NUMBER_SIZE 			1
#define BOOT_SEC_CURRENT_HEAD_FAT32_OFFSET 		0x041
#define BOOT_SEC_CURRENT_HEAD_FAT16_OFFSET 		0x025
#define BOOT_SEC_CURRENT_HEAD_SIZE 			1
#define BOOT_SEC_BOOT_SIGNATURE_FAT32_OFFSET 		0x042
#define BOOT_SEC_BOOT_SIGNATURE_FAT16_OFFSET 		0x026
#define BOOT_SEC_BOOT_SIGNATURE_SIZE 			1
#define BOOT_SEC_VOLUME_ID_FAT32_OFFSET 		0x043
#define BOOT_SEC_VOLUME_ID_FAT16_OFFSET 		0x027
#define BOOT_SEC_VOLUME_ID_SIZE 			4
#define BOOT_SEC_VOLUME_LABEL_FAT32_OFFSET 		0x047
#define BOOT_SEC_VOLUME_LABEL_FAT16_OFFSET 		0x02B
#define BOOT_SEC_VOLUME_LABEL_SIZE 			11
#define BOOT_SEC_FILE_SYS_TYPE_FAT32_OFFSET 		0x052
#define BOOT_SEC_FILE_SYS_TYPE_FAT16_OFFSET 		0x036
#define BOOT_SEC_FILE_SYS_TYPE_SIZE 			8
#define BOOT_SEC_BOOT_CODE_FAT32_OFFSET 		0x08A
#define BOOT_SEC_BOOT_CODE_FAT32_SIZE 			372
#define BOOT_SEC_BOOT_CODE_FAT16_OFFSET 		0x03E
#define BOOT_SEC_BOOT_CODE_FAT16_SIZE 			448
#define BOOT_SEC_BOOT_SECTOR_SIGNATURE_OFFSET 		0x1FE
#define BOOT_SEC_BOOT_SECTOR_SIGNATURE_SIZE 		2

class BootSector
{
	public:
		BootSector (uint8_t* offset, const PartitionType& partitionType = PartitionType::EMPTY);
		~BootSector();

		PartitionType getPartitionType() const;

		bool volumeIdLabelAndFSTypeAreValid() const;

		bool bootSectorSignatureIsValid() const;

		uint32_t 	getJumpInstruction() const;
		const char* 	getOemName() const;
		uint16_t 	getSectorSizeInBytes() const;
		uint8_t 	getNumSectorsPerCluster() const;
		uint16_t 	getNumReservedSectors() const;
		uint8_t 	getNumFats() const;
		uint16_t 	getNumDirectoryEntriesInRoot() const;
		uint32_t 	getNumSectorsOnDisk() const;
		uint8_t 	getMediaDescriptor() const;
		uint16_t 	getNumSectorsPerFat() const;
		uint16_t 	getNumSectorsPerTrack() const;
		uint16_t 	getNumHeads() const;
		uint32_t 	getNumHiddenSectors() const;
		uint8_t 	getDriveNum() const;
		uint8_t 	getCurrentHead() const;
		uint8_t 	getBootSignature() const;
		uint32_t 	getVolumeID() const;
		const char* 	getVolumeLabel() const;
		const char* 	getFileSystemType() const;
		uint16_t 	getBootSectorSignature() const;
		uint16_t 	getFlagsForFatMirroringAndActiveFat() const;
		uint16_t 	getFileSystemVersionNum() const;
		uint32_t 	getClusterNumForRoot() const;
		uint16_t 	getFSInfoSectorNum() const;
		uint16_t 	getBackupSectorNum() const;

	private:
		PartitionType 	m_PartitionType;

		uint32_t 	m_JumpInstruction; // never going to use this, but whatever
		char 	 	m_OemName[BOOT_SEC_OEM_NAME_SIZE];
		uint16_t 	m_SectorSizeInBytes;
		uint8_t 	m_NumSectorsPerCluster;
		uint16_t 	m_NumReservedSectors; // including boot sector
		uint8_t 	m_NumFats;
		uint16_t 	m_NumDirEntriesInRoot; // NA for FAT32
		uint32_t 	m_NumSectorsOnDisk;
		uint8_t 	m_MediaDescriptor;
		uint16_t 	m_NumSectorsPerFat; // NA for FAT32
		uint16_t 	m_NumSectorsPerTrack; // CHS addressing
		uint16_t 	m_NumHeads; // CHS addressing
		uint32_t 	m_NumHiddenSectors; // sectors before the boot sector
		uint8_t 	m_DriveNum;
		uint8_t 	m_CurrentHead;
		uint8_t 	m_BootSignature; // must be 0x29 for next 3 fields to be valid
		uint32_t 	m_VolumeID;
		char 		m_VolumeLabel[BOOT_SEC_VOLUME_LABEL_SIZE];
		char 		m_FileSystemType[BOOT_SEC_FILE_SYS_TYPE_SIZE];
		// uint8_t 	m_BootCode[448]; // not currently using this, as we don't actually plan to boot from SD card
		uint16_t 	m_BootSectorSignature; // must be 0xAA55

		// FAT32 only!
		uint32_t 	m_NumSectorsForFat;
		uint16_t 	m_FlagsForFatMirroringAndActiveFat;
		uint16_t 	m_FileSystemVersionNum;
		uint32_t 	m_ClusterNumForRoot;
		uint16_t 	m_FSInfoSectorNum;
		uint16_t 	m_BackupSectorNum;
};

#endif // BOOT_SECTOR_HPP
