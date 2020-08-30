#include "BootSector.hpp"

BootSector::BootSector (uint8_t* offset, const PartitionType& partitionType) :
	m_PartitionType( partitionType ),
	m_JumpInstruction( (offset[2] << 16) | (offset[1] << 8) | (offset[0]) ),
	m_OemName{ 0 }, // we'll do this in a loop in the constructor
	m_SectorSizeInBytes( (offset[BOOT_SEC_SECTOR_SIZE_OFFSET + 1] << 8) | (offset[BOOT_SEC_SECTOR_SIZE_OFFSET]) ),
	m_NumSectorsPerCluster( offset[BOOT_SEC_NUM_SECS_PER_CLUSTER_OFFSET] ),
	m_NumReservedSectors( (offset[BOOT_SEC_RESERVED_SECS_OFFSET + 1] << 8) | offset[BOOT_SEC_RESERVED_SECS_OFFSET] ),
	m_NumFats( offset[BOOT_SEC_NUM_FATS_OFFSET] ),
	m_NumDirEntriesInRoot( (offset[BOOT_SEC_NUM_DIRS_IN_ROOT_OFFSET + 1] << 8) | offset[BOOT_SEC_NUM_DIRS_IN_ROOT_OFFSET] ),
	m_NumSectorsOnDisk( (offset[BOOT_SEC_NUM_SECS_ON_DISK_LT_32MB_OFFSET + 1] << 8) // this may be zero, so we use logic in the
				| offset[BOOT_SEC_NUM_SECS_ON_DISK_LT_32MB_OFFSET] ), 	// constructor to find true size
	m_MediaDescriptor( offset[BOOT_SEC_MEDIA_DESCRIPTOR_OFFSET] ),
	m_NumSectorsPerFat( (offset[BOOT_SEC_NUM_SECS_PER_FAT_OFFSET + 1] << 8) | offset[BOOT_SEC_NUM_SECS_PER_FAT_OFFSET] ),
	m_NumSectorsPerTrack( (offset[BOOT_SEC_NUM_SECS_PER_TRACK_OFFSET + 1] << 8) | offset[BOOT_SEC_NUM_SECS_PER_TRACK_OFFSET] ),
	m_NumHeads( (offset[BOOT_SEC_NUM_HEADS_OFFSET + 1] << 8) | offset[BOOT_SEC_NUM_HEADS_OFFSET] ),
	m_NumHiddenSectors( (offset[BOOT_SEC_NUM_HIDDEN_SECS_OFFSET + 3] << 24)
				| (offset[BOOT_SEC_NUM_HIDDEN_SECS_OFFSET + 2] << 16)
				| (offset[BOOT_SEC_NUM_HIDDEN_SECS_OFFSET + 1] << 8)
				| offset[BOOT_SEC_NUM_HIDDEN_SECS_OFFSET] ),
	m_DriveNum( offset[BOOT_SEC_DRIVE_NUMBER_FAT16_OFFSET] ),
	m_CurrentHead( offset[BOOT_SEC_CURRENT_HEAD_FAT16_OFFSET] ),
	m_BootSignature( offset[BOOT_SEC_BOOT_SIGNATURE_FAT16_OFFSET] ),
	m_VolumeID( (offset[BOOT_SEC_VOLUME_ID_FAT16_OFFSET + 3] << 24)
				| (offset[BOOT_SEC_VOLUME_ID_FAT16_OFFSET + 2] << 16)
				| (offset[BOOT_SEC_VOLUME_ID_FAT16_OFFSET + 1] << 8)
				| offset[BOOT_SEC_VOLUME_ID_FAT16_OFFSET] ),
	m_VolumeLabel{ 0 },
	m_FileSystemType{ 0 },
	// ignoring boot code
	m_BootSectorSignature( (offset[BOOT_SEC_BOOT_SECTOR_SIGNATURE_OFFSET + 1] << 8)
				| offset[BOOT_SEC_BOOT_SECTOR_SIGNATURE_OFFSET] ),
	m_NumSectorsForFat( (offset[BOOT_SEC_NUM_SECS_PER_FAT_FAT32_OFFSET + 3] << 24)
				| (offset[BOOT_SEC_NUM_SECS_PER_FAT_FAT32_OFFSET + 2] << 16)
				| (offset[BOOT_SEC_NUM_SECS_PER_FAT_FAT32_OFFSET + 1] << 8)
				| offset[BOOT_SEC_NUM_SECS_PER_FAT_FAT32_OFFSET] ),
	m_FlagsForFatMirroringAndActiveFat( (offset[BOOT_SEC_FAT_MIRR_ACTIVE_FAT_FAT32_OFFSET + 1] << 8)
						| offset[BOOT_SEC_FAT_MIRR_ACTIVE_FAT_FAT32_OFFSET] ),
	m_FileSystemVersionNum( (offset[BOOT_SEC_FILE_SYSTEM_VER_FAT32_OFFSET + 1] << 8)
				| offset[BOOT_SEC_FILE_SYSTEM_VER_FAT32_OFFSET] ),
	m_ClusterNumForRoot( (offset[BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_OFFSET + 3] << 24)
				| (offset[BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_OFFSET + 2] << 16)
				| (offset[BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_OFFSET + 1] << 8)
				| offset[BOOT_SEC_CLUSTER_NUM_FOR_ROOT_FAT32_OFFSET] ),
	m_FSInfoSectorNum( (offset[BOOT_SEC_FSINFO_SEC_NUM_FAT32_OFFSET + 1] << 8)
				| offset[BOOT_SEC_FSINFO_SEC_NUM_FAT32_OFFSET] ),
	m_BackupSectorNum( (offset[BOOT_SEC_BACKUP_SEC_NUM_FAT32_OFFSET + 1] << 8)
						| offset[BOOT_SEC_BACKUP_SEC_NUM_FAT32_OFFSET] )
{
	// get the OEM name
	for ( unsigned int character = 0; character < BOOT_SEC_OEM_NAME_SIZE; character++ )
	{
		m_OemName[character] = offset[BOOT_SEC_OEM_NAME_OFFSET + character];
	}

	// if num sectors is 0, the partition is likely > 32MB and we need to check other offset
	if ( m_NumSectorsOnDisk == 0 )
	{
		m_NumSectorsOnDisk = (offset[BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_OFFSET + 3] << 24)
					| (offset[BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_OFFSET + 2] << 16)
					| (offset[BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_OFFSET + 1] << 8)
					| offset[BOOT_SEC_NUM_SECS_ON_DISK_GT_32MB_OFFSET];
	}

	// if the file system type isn't FAT16 or FAT12, we need to use FAT32 offsets
	if ( partitionType == PartitionType::FAT32_LTOREQ_2GB || partitionType == PartitionType::FAT32_LBA )
	{
		m_DriveNum = offset[BOOT_SEC_DRIVE_NUMBER_FAT32_OFFSET];
		m_CurrentHead = offset[BOOT_SEC_CURRENT_HEAD_FAT32_OFFSET];
		m_BootSignature = offset[BOOT_SEC_BOOT_SIGNATURE_FAT32_OFFSET];
		m_VolumeID = (offset[BOOT_SEC_VOLUME_ID_FAT32_OFFSET + 3] << 24)
				| (offset[BOOT_SEC_VOLUME_ID_FAT32_OFFSET + 2] << 16)
				| (offset[BOOT_SEC_VOLUME_ID_FAT32_OFFSET + 1] << 8)
				| offset[BOOT_SEC_VOLUME_ID_FAT32_OFFSET];

		// get the file system type name from the FAT32 offset
		for ( unsigned int character = 0; character < BOOT_SEC_FILE_SYS_TYPE_SIZE; character++ )
		{
			m_FileSystemType[character] = offset[BOOT_SEC_FILE_SYS_TYPE_FAT32_OFFSET + character];
		}

		// get the volume label from the FAT32 offset
		for ( unsigned int character = 0; character < BOOT_SEC_VOLUME_LABEL_SIZE; character++ )
		{
			m_VolumeLabel[character] = offset[BOOT_SEC_VOLUME_LABEL_FAT32_OFFSET + character];
		}
	}
	else // a FAT16 or FAT12 file system
	{

		// get the file system type name from the FAT16/FAT12 offset
		for ( unsigned int character = 0; character < BOOT_SEC_FILE_SYS_TYPE_SIZE; character++ )
		{
			m_FileSystemType[character] = offset[BOOT_SEC_FILE_SYS_TYPE_FAT16_OFFSET + character];
		}

		// get the volume label from the FAT16/FAT12 offset
		for ( unsigned int character = 0; character < BOOT_SEC_VOLUME_LABEL_SIZE; character++ )
		{
			m_VolumeLabel[character] = offset[BOOT_SEC_VOLUME_LABEL_FAT16_OFFSET + character];
		}
	}
}

BootSector::~BootSector()
{
}

PartitionType BootSector::getPartitionType() const
{
	return m_PartitionType;
}

bool BootSector::volumeIdLabelAndFSTypeAreValid() const
{
	if ( m_BootSignature == 0x29 )
	{
		return true;
	}

	return false;
}

bool BootSector::bootSectorSignatureIsValid() const
{
	if ( m_BootSectorSignature == 0xAA55 )
	{
		return true;
	}

	return false;
}

uint32_t BootSector::getJumpInstruction() const
{
	return m_JumpInstruction;
}

const char* BootSector::getOemName() const
{
	return m_OemName;
}

uint16_t BootSector::getSectorSizeInBytes() const
{
	return m_SectorSizeInBytes;
}

uint8_t BootSector::getNumSectorsPerCluster() const
{
	return m_NumSectorsPerCluster;
}

uint16_t BootSector::getNumReservedSectors() const
{
	return m_NumReservedSectors;
}

uint8_t BootSector::getNumFats() const
{
	return m_NumFats;
}

uint16_t BootSector::getNumDirectoryEntriesInRoot() const
{
	return m_NumDirEntriesInRoot;
}

uint32_t BootSector::getNumSectorsOnDisk() const
{
	return m_NumSectorsOnDisk;
}

uint8_t BootSector::getMediaDescriptor() const
{
	return m_MediaDescriptor;
}

uint16_t BootSector::getNumSectorsPerFat() const
{
	if ( m_PartitionType == PartitionType::FAT32_LTOREQ_2GB
			|| m_PartitionType == PartitionType::FAT32_LBA )
	{
		return m_NumSectorsForFat;
	}

	return m_NumSectorsPerFat;
}

uint16_t BootSector::getNumSectorsPerTrack() const
{
	return m_NumSectorsPerTrack;
}

uint16_t BootSector::getNumHeads() const
{
	return m_NumHeads;
}

uint32_t BootSector::getNumHiddenSectors() const
{
	return m_NumHiddenSectors;
}

uint8_t BootSector::getDriveNum() const
{
	return m_DriveNum;
}

uint8_t BootSector::getCurrentHead() const
{
	return m_CurrentHead;
}

uint8_t BootSector::getBootSignature() const
{
	return m_BootSignature;
}

uint32_t BootSector::getVolumeID() const
{
	return m_VolumeID;
}

const char* BootSector::getVolumeLabel() const
{
	return m_VolumeLabel;
}

const char* BootSector::getFileSystemType() const
{
	return m_FileSystemType;
}

uint16_t BootSector::getBootSectorSignature() const
{
	return m_BootSectorSignature;
}

uint16_t BootSector::getFlagsForFatMirroringAndActiveFat() const
{
	return m_FlagsForFatMirroringAndActiveFat;
}

uint16_t BootSector::getFileSystemVersionNum() const
{
	return m_FileSystemVersionNum;
}

uint32_t BootSector::getClusterNumForRoot() const
{
	return m_ClusterNumForRoot;
}

uint16_t BootSector::getFSInfoSectorNum() const
{
	return m_FSInfoSectorNum;
}

uint16_t BootSector::getBackupSectorNum() const
{
	return m_BackupSectorNum;
}
