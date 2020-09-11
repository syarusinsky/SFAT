#include "Fat16FileManager.hpp"

Fat16FileManager::Fat16FileManager (IStorageMedia& storageMedia) :
	IFatFileManager( storageMedia ),
	m_FatOffset( 0 ),
	m_RootDirectoryOffset( 0 ),
	m_DataOffset( 0 ),
	m_CurrentOffset( 0 ),
	m_CurrentDirectoryEntries(),
	m_FileTransferInProgress( false ),
	m_CurrentFileSector( 0 ),
	m_CurrentFileCluster( 0 )
{
	if ( this->isValidFatFileSystem() )
	{
		m_FatOffset = ( m_PartitionTables.at(m_ActivePartitionNum).getOffsetLBA() + m_ActiveBootSector->getNumReservedSectors() ) *
						m_ActiveBootSector->getSectorSizeInBytes();

		// make the current entry offset the root directory offset and load the current entry sector with root directory entries
		m_RootDirectoryOffset = m_FatOffset + ( (m_ActiveBootSector->getNumFats() * m_ActiveBootSector->getNumSectorsPerFat() ) *
						m_ActiveBootSector->getSectorSizeInBytes() );
		m_CurrentOffset = m_RootDirectoryOffset;

		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( FAT16_ENTRY_SIZE * m_ActiveBootSector->getNumDirectoryEntriesInRoot(),
								m_RootDirectoryOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}

		m_DataOffset = m_RootDirectoryOffset + ( m_ActiveBootSector->getNumDirectoryEntriesInRoot() * FAT16_ENTRY_SIZE );
	}
}

Fat16FileManager::~Fat16FileManager()
{
}

Fat16Entry Fat16FileManager::selectEntry (unsigned int entryNum)
{
	// TODO we probably want to keep our current entry offset (for entries) separate from any file buffer offsets (which need to look up FATS)
	Fat16Entry entry( m_CurrentDirectoryEntries.at(entryNum) );

	if ( entry.isRootDirectory() )
	{
		m_FileTransferInProgress = false;

		m_CurrentDirectoryEntries.clear();

		m_CurrentOffset = m_RootDirectoryOffset;

		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( FAT16_ENTRY_SIZE * m_ActiveBootSector->getNumDirectoryEntriesInRoot(),
								m_RootDirectoryOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
	else if ( entry.isSubdirectory() )
	{
		// TODO need to account for big directories where one sector isn't enough to hold them

		m_FileTransferInProgress = false;

		m_CurrentDirectoryEntries.clear();

		m_CurrentOffset = m_DataOffset + ( (entry.getStartingClusterNum() - 2) *
						m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(), m_CurrentOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
	else if ( ! entry.isUnusedEntry()
			&& ! entry.isDeletedEntry()
			&& ! entry.isHiddenEntry()
			&& ! entry.isSystemFile()
			&& ! entry.isDiskVolumeLabel() )
	{
		m_FileTransferInProgress = true;
		m_CurrentFileSector = 0;
		m_CurrentFileCluster = entry.getStartingClusterNum();

		// move offset to first cluster of the file
		m_CurrentOffset = m_DataOffset + ( (m_CurrentFileCluster - 2) *
						m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );
	}

	return entry;
}

SharedData<uint8_t> Fat16FileManager::getSelectedFileNextSector()
{
	if ( m_FileTransferInProgress )
	{
		unsigned int returnOffset = m_CurrentOffset;

		m_CurrentFileSector++;
		if ( m_CurrentFileSector == m_ActiveBootSector->getNumSectorsPerCluster() )
		{
			m_CurrentFileSector = 0;

			// look up the next cluster in the FAT
			// TODO might want to make the FAT a data structure instead? (almost definitely)
			unsigned int nextClusterOffset = m_FatOffset + ( sizeof(uint16_t) * m_CurrentFileCluster );

			SharedData<uint8_t> nextClusterData = m_StorageMedia.readFromMedia( sizeof(uint16_t), nextClusterOffset );
			uint16_t* nextCluster = reinterpret_cast<uint16_t*>( nextClusterData.getPtr() );

			m_CurrentFileCluster = *nextCluster;

			if ( m_CurrentFileCluster == FAT16_END_OF_FILE_CLUSTER )
			{
				m_FileTransferInProgress = false;
			}
		}

		m_CurrentOffset = m_DataOffset + ( (m_CurrentFileCluster - 2) *
					m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() ) +
					( m_CurrentFileSector * m_ActiveBootSector->getSectorSizeInBytes() );

		return m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(), returnOffset );
	}

	return SharedData<uint8_t>::MakeSharedDataNull();
}

void Fat16FileManager::changePartition (unsigned int partitionNum)
{
	IFatFileManager::changePartition( partitionNum );

	m_RootDirectoryOffset = ( m_PartitionTables.at(m_ActivePartitionNum).getOffsetLBA() + m_ActiveBootSector->getNumReservedSectors() +
					m_ActiveBootSector->getNumFats() * m_ActiveBootSector->getNumSectorsPerFat() ) *
					m_ActiveBootSector->getSectorSizeInBytes();

	m_CurrentDirectoryEntries.clear();

	m_CurrentOffset = m_RootDirectoryOffset;

	SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( FAT16_ENTRY_SIZE * m_ActiveBootSector->getNumDirectoryEntriesInRoot(),
								m_RootDirectoryOffset );
	// fill directory entries vector
	uint8_t* entriesPtr = entries.getPtr();
	for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
	{
		m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
	}
}
