#include "Fat16Entry.hpp"

#include <string.h>

Fat16Entry::Fat16Entry (uint8_t* offset) :
	m_UnderlyingData{ 0 },
	m_Filename{ 0 },
	m_Extension{ 0 },
	m_FileAttributes( offset[FAT16_ATTRIBUTES_OFFSET] ),
	m_TimeLastUpdated( (offset[FAT16_TIME_LAST_UPDATED_OFFSET + 1] << 8) | offset[FAT16_TIME_LAST_UPDATED_OFFSET] ),
	m_DateLastUpdated( (offset[FAT16_DATE_LAST_UPDATED_OFFSET + 1] << 8) | offset[FAT16_DATE_LAST_UPDATED_OFFSET] ),
	m_StartingClusterNum( (offset[FAT16_STARTING_CLUSTER_NUM_OFFSET + 1] << 8) | offset[FAT16_STARTING_CLUSTER_NUM_OFFSET] ),
	m_FileSizeInBytes( (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 3] << 24)
				| (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 2] << 16)
				| (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 1] << 8)
				| offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET] ),
	m_IsInvalidEntry( false ),
	m_FileTransferInProgress( false ),
	m_CurrentFileSector( 0 ),
	m_CurrentFileCluster( 0 ),
	m_CurrentDirOffset( 0 ),
	m_CurrentFileOffset( 0 ),
	m_NumBytesRead( 0 ),
	m_ClustersToModify()
{
	for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
	{
		m_UnderlyingData[byte] = offset[byte];
	}

	for ( unsigned int character = 0; character < FAT16_FILENAME_SIZE; character++ )
	{
		m_Filename[character] = offset[FAT16_FILENAME_OFFSET + character];
	}

	for ( unsigned int character = 0; character < FAT16_EXTENSION_SIZE; character++ )
	{
		m_Extension[character] = offset[FAT16_EXTENSION_OFFSET + character];
	}

	this->createFilenameDisplayString();
}

Fat16Entry::Fat16Entry (const std::string& filename, const std::string& extension)
{
	if ( filename.size() > FAT16_FILENAME_SIZE )
	{
		m_IsInvalidEntry = true;
	}
	else if ( extension.size() > FAT16_EXTENSION_SIZE )
	{
		m_IsInvalidEntry = true;
	}
	else
	{
		uint8_t underlyingData[FAT16_ENTRY_SIZE] = { 0 };

		// write filename
		unsigned int byte = 0;
		for ( const char& character : filename )
		{
			underlyingData[FAT16_FILENAME_OFFSET + byte] = static_cast<uint8_t>( character );
			byte++;
		}
		while ( byte < FAT16_FILENAME_SIZE )
		{
			underlyingData[FAT16_FILENAME_OFFSET + byte] = static_cast<uint8_t>( ' ' );
			byte++;
		}

		// write extension
		byte = 0;
		for ( const char& character : extension )
		{
			underlyingData[FAT16_EXTENSION_OFFSET + byte] = static_cast<uint8_t>( character );
			byte++;
		}
		while ( byte < FAT16_EXTENSION_SIZE )
		{
			underlyingData[FAT16_EXTENSION_OFFSET + byte] = static_cast<uint8_t>( ' ' );
			byte++;
		}

		*this = Fat16Entry( underlyingData );
	}
}

Fat16Entry::Fat16Entry (const Fat16Entry& other) :
	m_UnderlyingData{ 0 },
	m_Filename{ 0 },
	m_Extension{ 0 },
	m_FilenameWithExtension{ 0 },
	m_FileAttributes( other.m_FileAttributes ),
	m_TimeLastUpdated( other.m_TimeLastUpdated ),
	m_DateLastUpdated( other.m_DateLastUpdated ),
	m_StartingClusterNum( other.m_StartingClusterNum ),
	m_FileSizeInBytes( other.m_FileSizeInBytes ),
	m_IsInvalidEntry( other.m_IsInvalidEntry ),
	m_FileTransferInProgress( false ),
	m_CurrentFileSector( 0 ),
	m_CurrentFileCluster( 0 ),
	m_CurrentDirOffset( 0 ),
	m_CurrentFileOffset( 0 ),
	m_ClustersToModify()
{
	for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
	{
		m_UnderlyingData[byte] = other.m_UnderlyingData[byte];
	}

	for ( unsigned int byte = 0; byte < FAT16_FILENAME_SIZE; byte++ )
	{
		m_Filename[byte] = other.m_Filename[byte];
	}

	for ( unsigned int byte = 0; byte < FAT16_EXTENSION_SIZE; byte++ )
	{
		m_Extension[byte] = other.m_Extension[byte];
	}

	for ( unsigned int byte = 0; byte < FAT16_FILENAME_SIZE + FAT16_EXTENSION_SIZE + 2; byte++ ) // plus 2 for . and string terminator
	{
		m_FilenameWithExtension[byte] = other.m_FilenameWithExtension[byte];
	}
}

Fat16Entry::~Fat16Entry()
{
}

void Fat16Entry::setToDeleted()
{
	m_Filename[0] = static_cast<char>( 0xE5 );
	m_UnderlyingData[FAT16_FILENAME_OFFSET] = 0xE5;
}

void Fat16Entry::setStartingClusterNum (uint16_t clusterNum)
{
	uint8_t byte1 = ( clusterNum & 0xFF00 ) >> 8;
	uint8_t byte2 = clusterNum & 0x00FF;
	m_UnderlyingData[FAT16_STARTING_CLUSTER_NUM_OFFSET] = byte2;
	m_UnderlyingData[FAT16_STARTING_CLUSTER_NUM_OFFSET + 1] = byte1;
	m_StartingClusterNum = ( m_UnderlyingData[FAT16_STARTING_CLUSTER_NUM_OFFSET + 1] << 8 )
				| m_UnderlyingData[FAT16_STARTING_CLUSTER_NUM_OFFSET];
}

void Fat16Entry::setFileSizeInBytes (uint32_t fileSize)
{
	uint8_t byte1 = ( fileSize & 0xFF000000 ) >> 24;
	uint8_t byte2 = ( fileSize & 0x00FF0000 ) >> 16;
	uint8_t byte3 = ( fileSize & 0x0000FF00 ) >> 8;
	uint8_t byte4 = fileSize & 0x000000FF;
	m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET] = byte4;
	m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 1] = byte3;
	m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 2] = byte2;
	m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 3] = byte1;
	m_FileSizeInBytes = ( m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 3] << 24 )
				| ( m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 2] << 16 )
				| ( m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 1] << 8 )
				| m_UnderlyingData[FAT16_FILE_SIZE_IN_BYTES_OFFSET];
}

const uint8_t* Fat16Entry::getUnderlyingData() const
{
	return m_UnderlyingData;
}

const char* Fat16Entry::getFilenameRaw() const
{
	return m_Filename;
}

const char* Fat16Entry::getExtensionRaw() const
{
	return m_Extension;
}

const char* Fat16Entry::getFilenameDisplay()
{
	// since the filename or extension may have been modified, we need to recreate the display string
	this->createFilenameDisplayString();

	return m_FilenameWithExtension;
}

const char* Fat16Entry::getFilenameDisplay() const
{
	return m_FilenameWithExtension;
}

uint8_t Fat16Entry::getFileAttributesRaw() const
{
	return m_FileAttributes;
}

uint16_t Fat16Entry::getTimeUpdatedRaw() const
{
	return m_TimeLastUpdated;
}

const Fat16Time Fat16Entry::getTimeUpdated() const
{
	Fat16Time time;
	time.hours = m_TimeLastUpdated >> 11;
	time.minutes = ( m_TimeLastUpdated & 0b0000011111100000 ) >> 5;
	time.twoSecondIntervals = m_TimeLastUpdated & 0b0000000000011111;

	return time;
}

uint16_t Fat16Entry::getDateUpdatedRaw() const
{
	return m_DateLastUpdated;
}

const Fat16Date Fat16Entry::getDateUpdated() const
{
	Fat16Date date;
	date.year = m_DateLastUpdated >> 9;
	date.month = ( m_DateLastUpdated & 0b0000000111100000 ) >> 5;
	date.day = m_DateLastUpdated & 0b0000000000011111;

	return date;
}

uint16_t Fat16Entry::getStartingClusterNum() const
{
	return m_StartingClusterNum;
}

uint32_t Fat16Entry::getFileSizeInBytes() const
{
	return m_FileSizeInBytes;
}

bool Fat16Entry::isUnusedEntry() const
{
	if ( m_Filename[0] == static_cast<char>(0x00) )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isDeletedEntry() const
{
	if ( m_Filename[0] == static_cast<char>(0xE5) )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isDirectory() const
{
	if ( m_Filename[0] == static_cast<char>(0x2E) )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isRootDirectory() const
{
	// TODO is this right?
	if ( m_Filename[0] == static_cast<char>(0x2E) && m_StartingClusterNum == 0 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::clusterNumIsParentDirectory() const
{
	if ( m_Filename[0] == static_cast<char>(0x2E) && m_Filename[1] == static_cast<char>(0x2E) )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isReadOnly() const
{
	if ( m_FileAttributes & 0x01 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isHiddenEntry() const
{
	if ( m_FileAttributes & 0x02 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isSystemFile() const
{
	if ( m_FileAttributes & 0x04 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isDiskVolumeLabel() const
{
	if ( m_FileAttributes & 0x08 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isSubdirectory() const
{
	if ( m_FileAttributes & 0x10 )
	{
		return true;
	}

	return false;
}

bool Fat16Entry::isInvalidEntry() const
{
	return m_IsInvalidEntry;
}

void Fat16Entry::createFilenameDisplayString()
{
	if ( this->isUnusedEntry() || this->isDeletedEntry() )
	{
		m_FilenameWithExtension[0] = '\0';
	}
	else if ( this->isRootDirectory() )
	{
		m_FilenameWithExtension[0] = '/';
	}
	else if ( this->isDirectory() && this->clusterNumIsParentDirectory() )
	{
		m_FilenameWithExtension[0] = '.';
		m_FilenameWithExtension[1] = '.';
		m_FilenameWithExtension[2] = '\0';
	}
	else if ( m_Filename[0] == 0x05 )
	{
		m_FilenameWithExtension[0] = 0xE5;

		this->createFilenameDisplayStringHelper( 1 );
	}
	else
	{
		this->createFilenameDisplayStringHelper( 0 );
	}
}

void Fat16Entry::createFilenameDisplayStringHelper (unsigned int startCharacter)
{
		unsigned int endOfFilename = startCharacter;

		// fill the filename part
		for ( unsigned int character = endOfFilename; character < FAT16_FILENAME_SIZE; character++ )
		{
			if ( m_Filename[character] == ' ' )
			{
				break;
			}

			m_FilenameWithExtension[endOfFilename] = m_Filename[character];
			endOfFilename++;
		}

		if ( ! this->isSubdirectory() )
		{
			// add . for extension
			m_FilenameWithExtension[endOfFilename] = '.';
			endOfFilename++;

			// fill the extension part
			for ( unsigned int character = 0; character < FAT16_EXTENSION_SIZE; character++ )
			{
				if ( m_Extension[character] == ' ' )
				{
					break;
				}

				m_FilenameWithExtension[endOfFilename] = m_Extension[character];
				endOfFilename++;
			}
		}

		// add string terminator
		m_FilenameWithExtension[endOfFilename] = '\0';
}
