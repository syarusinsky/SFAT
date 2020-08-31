#include "Fat16Entry.hpp"

#include <string.h>

Fat16Entry::Fat16Entry (uint8_t* offset) :
	m_Filename{ 0 },
	m_Extension{ 0 },
	m_FileAttributes( offset[FAT16_ATTRIBUTES_OFFSET] ),
	m_TimeLastUpdated( (offset[FAT16_TIME_LAST_UPDATED_OFFSET + 1] << 8) | offset[FAT16_TIME_LAST_UPDATED_OFFSET] ),
	m_DateLastUpdated( (offset[FAT16_DATE_LAST_UPDATED_OFFSET + 1] << 8) | offset[FAT16_DATE_LAST_UPDATED_OFFSET] ),
	m_StartingClusterNum( (offset[FAT16_STARTING_CLUSTER_NUM_OFFSET + 1] << 8) | offset[FAT16_STARTING_CLUSTER_NUM_OFFSET] ),
	m_FileSizeInBytes( (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 3] << 24)
				| (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 2] << 16)
				| (offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET + 1] << 8)
				| offset[FAT16_FILE_SIZE_IN_BYTES_OFFSET] )
{
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

Fat16Entry::~Fat16Entry()
{
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
