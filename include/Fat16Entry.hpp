#ifndef FAT16ENTRY_HPP
#define FAT16ENTRY_HPP

/**************************************************************************
 * The Fat16Entry class defines a directory entry for a FAT16 file
 * system. It provides functions for determining what type of entry
 * it is and also acts as a 'handle' to fat16 files. You can use these
 * handles to initiate file read or write sequences.
**************************************************************************/

#include <stdint.h>
#include <string>
#include <vector>

// FAT16 entry offsets and sizes
#define FAT16_ENTRY_SIZE 			32
#define FAT16_FILENAME_OFFSET 			0x00
#define FAT16_FILENAME_SIZE 			8
#define FAT16_EXTENSION_OFFSET 			0x08
#define FAT16_EXTENSION_SIZE 			3
#define FAT16_ATTRIBUTES_OFFSET 		0x0B
#define FAT16_ATTRIBUTES_SIZE 			1
#define FAT16_TIME_LAST_UPDATED_OFFSET 		0x16
#define FAT16_TIME_LAST_UPDATED_SIZE 		2
#define FAT16_DATE_LAST_UPDATED_OFFSET 		0x18
#define FAT16_DATE_LAST_UPDATED_SIZE 		2
#define FAT16_STARTING_CLUSTER_NUM_OFFSET 	0x1A
#define FAT16_STARTING_CLUSTER_NUM_SIZE 	2
#define FAT16_FILE_SIZE_IN_BYTES_OFFSET 	0x1C
#define FAT16_FILE_SIZE_IN_BYTES_SIZE 		4
#define FAT16_END_OF_FILE_CLUSTER 		0xFFFF
#define FAT16_BAD_CLUSTER 			0xFFF7
#define FAT16_FREE_CLUSTER 			0x0000
#define FAT16_RESERVED_CLUSTER 			0x0001

struct Fat16Time
{
	uint8_t hours;
	uint8_t minutes;
	uint8_t twoSecondIntervals;
};

struct Fat16Date
{
	uint8_t year;
	uint8_t month;
	uint8_t day;
};

struct Fat16ClusterMod
{
	uint16_t clusterNum;
	uint16_t clusterNewVal;
};

class Fat16Entry
{
	public:
		Fat16Entry (uint8_t* offset);
		Fat16Entry (const std::string& filename, const std::string& extension);
		Fat16Entry (const Fat16Entry& other);
		~Fat16Entry();

		void setToDeleted();

		void setStartingClusterNum (uint16_t clusterNum);
		void setFileSizeInBytes (uint32_t fileSize);

		const uint8_t* getUnderlyingData() const;

		const char* getFilenameRaw() const; // can contain special characters, so don't use this for display purposes
		const char* getExtensionRaw() const;

		const char* getFilenameDisplay();
		const char* getFilenameDisplay() const; // use this for display purposes

		uint8_t getFileAttributesRaw() const;

		uint16_t getTimeUpdatedRaw() const;
		const Fat16Time getTimeUpdated() const;

		uint16_t getDateUpdatedRaw() const;
		const Fat16Date getDateUpdated() const;

		uint16_t getStartingClusterNum() const;

		uint32_t getFileSizeInBytes() const;

		bool isUnusedEntry() const;
		bool isDeletedEntry() const;
		bool isDirectory() const;
		bool isRootDirectory() const;
		bool clusterNumIsParentDirectory() const;
		bool isReadOnly() const;
		bool isHiddenEntry() const;
		bool isSystemFile() const;
		bool isDiskVolumeLabel() const;
		bool isSubdirectory() const;

		bool isInvalidEntry() const;

		bool& getFileTransferInProgressFlagRef() { return m_FileTransferInProgress; }
		unsigned int& getCurrentFileSectorRef() { return m_CurrentFileSector; }
		unsigned int& getCurrentFileClusterRef() { return m_CurrentFileCluster; }
		unsigned int& getCurrentDirOffsetRef() { return m_CurrentDirOffset; }
		unsigned int& getCurrentFileOffsetRef() { return m_CurrentFileOffset; }
		unsigned int& getNumBytesReadRef() { return m_NumBytesRead; }
		std::vector<Fat16ClusterMod>& getClustersToModifyRef() { return m_ClustersToModify; }

	private:
		uint8_t 	m_UnderlyingData[FAT16_ENTRY_SIZE];
		char 		m_Filename[FAT16_FILENAME_SIZE];
		char 		m_Extension[FAT16_EXTENSION_SIZE];
		char 		m_FilenameWithExtension[FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2]; // plus 2 for . and string terminator
		uint8_t 	m_FileAttributes;
		uint16_t 	m_TimeLastUpdated;
		uint16_t 	m_DateLastUpdated;
		uint16_t 	m_StartingClusterNum;
		uint32_t 	m_FileSizeInBytes;

		bool 		m_IsInvalidEntry;

		bool 		m_FileTransferInProgress;
		unsigned int 	m_CurrentFileSector;
		unsigned int 	m_CurrentFileCluster;
		unsigned int 	m_CurrentDirOffset;
		unsigned int 	m_CurrentFileOffset;
		unsigned int 	m_NumBytesRead = 0;

		std::vector<Fat16ClusterMod> 	m_ClustersToModify;

		void createFilenameDisplayString();
		void createFilenameDisplayStringHelper (unsigned int startCharacter);
};

#endif // FAT16ENTRY_HPP
