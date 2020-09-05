#ifndef FAT16ENTRY_HPP
#define FAT16ENTRY_HPP

/**************************************************************************
 * The Fat16Entry class defines a directory entry for a FAT16 file
 * system. It also provides function for determining what type of entry
 * it is.
**************************************************************************/

#include <stdint.h>

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

class Fat16Entry
{
	public:
		Fat16Entry (uint8_t* offset);
		~Fat16Entry();

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

	private:
		char 		m_Filename[FAT16_FILENAME_SIZE];
		char 		m_Extension[FAT16_EXTENSION_SIZE];
		char 		m_FilenameWithExtension[FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2]; // plus 2 for . and string terminator
		uint8_t 	m_FileAttributes;
		uint16_t 	m_TimeLastUpdated;
		uint16_t 	m_DateLastUpdated;
		uint16_t 	m_StartingClusterNum;
		uint32_t 	m_FileSizeInBytes;

		void createFilenameDisplayString();
		void createFilenameDisplayStringHelper (unsigned int startCharacter);
};

#endif // FAT16ENTRY_HPP
