#include "Fat16FileManager.hpp"

#include <algorithm>

Fat16FileManager::Fat16FileManager (IStorageMedia& storageMedia) :
	IFatFileManager( storageMedia ),
	m_FatOffset( 0 ),
	m_FatCached( SharedData<uint8_t>::MakeSharedData(1) ),
	m_RootDirectoryOffset( 0 ),
	m_DataOffset( 0 ),
	m_CurrentDirOffset( 0 ),
	m_CurrentDirectoryEntries()
{
	if ( this->isValidFatFileSystem() )
	{
		unsigned int partitionOffset = 0;
		if ( ! m_PartitionTables.empty() )
		{
			partitionOffset = m_PartitionTables.at( m_ActivePartitionNum ).getOffsetLBA();
		}

		m_FatOffset = ( partitionOffset + m_ActiveBootSector->getNumReservedSectors() ) * m_ActiveBootSector->getSectorSizeInBytes();

		m_FatCached = m_StorageMedia.readFromMedia( m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes(),
								m_FatOffset );

		// make the current entry offset the root directory offset and load the current entry sector with root directory entries
		m_RootDirectoryOffset = m_FatOffset + ( (m_ActiveBootSector->getNumFats() * m_ActiveBootSector->getNumSectorsPerFat() ) *
						m_ActiveBootSector->getSectorSizeInBytes() );
		m_CurrentDirOffset = m_RootDirectoryOffset;

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
	Fat16Entry entry( m_CurrentDirectoryEntries.at(entryNum) );

	if ( entry.isRootDirectory() )
	{
		m_CurrentDirectoryEntries.clear();

		m_CurrentDirOffset = m_RootDirectoryOffset;

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

		m_CurrentDirectoryEntries.clear();

		m_CurrentDirOffset = m_DataOffset + ( (entry.getStartingClusterNum() - 2) *
						m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(), m_CurrentDirOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}

	return entry;
}

bool Fat16FileManager::deleteEntry (unsigned int entryNum)
{
	if ( entryNum >= m_ActiveBootSector->getNumDirectoryEntriesInRoot() ) return false;

	Fat16Entry& entry( m_CurrentDirectoryEntries.at(entryNum) );

	if ( entry.isRootDirectory() ) return false;
	else if ( entry.isSubdirectory() ) return false; // TODO eventually implement subdirectory deleting
	else if ( entry.isDirectory() ) return false; // TODO eventually implement directory deleting
	else if ( entry.isDeletedEntry() ) return false;
	else if ( entry.isUnusedEntry() ) return false;
	else if ( entry.isReadOnly() ) return false;
	else if ( entry.isHiddenEntry() ) return false; // TODO in the future probably want to be able to delete hidden entries
	else if ( entry.isSystemFile() ) return false;
	else if ( entry.isDiskVolumeLabel() ) return false;

	entry.setToDeleted();

	// set the cluster chain to unused
	uint16_t cluster = entry.getStartingClusterNum();
	while ( cluster != FAT16_END_OF_FILE_CLUSTER
			&& cluster != FAT16_BAD_CLUSTER
			&& cluster != FAT16_RESERVED_CLUSTER )
	{
		// look up the next cluster in the FAT
		uint8_t* prevClusterByte1 = &m_FatCached[sizeof(uint16_t) * cluster];
		uint8_t* prevClusterByte2 = &m_FatCached[sizeof(uint16_t) * cluster + 1];
		uint16_t prevCluster = *prevClusterByte1 | ( *prevClusterByte2 << 8 );
		cluster = prevCluster;

		// set previous cluster to free
		*prevClusterByte1 = ( FAT16_FREE_CLUSTER & 0x00FF );
		*prevClusterByte2 = ( FAT16_FREE_CLUSTER & 0xFF00 ) >> 8;
	}

	// write entry back
	SharedData<uint8_t> entryData = SharedData<uint8_t>::MakeSharedData( FAT16_ENTRY_SIZE );
	unsigned int bytesWritten = 0;
	const uint8_t* underlyingData = entry.getUnderlyingData();
	for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
	{
		entryData[bytesWritten] = underlyingData[byte];
		bytesWritten++;
	}

	m_StorageMedia.writeToMedia( entryData, m_CurrentDirOffset + (entryNum * FAT16_ENTRY_SIZE) );

	// write FATs back (second for redundancy)
	m_StorageMedia.writeToMedia( m_FatCached, m_FatOffset );
	m_StorageMedia.writeToMedia( m_FatCached, m_FatOffset
							+ (m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes()) );

	return true;
}

bool Fat16FileManager::readEntry (Fat16Entry& entry)
{
	this->endFileTransfer( entry );

	// ensure this file exists in the current directory
	bool foundEntry = false;
	const uint8_t* underlyingData = entry.getUnderlyingData();
	for ( const Fat16Entry& listEntry : m_CurrentDirectoryEntries )
	{
		const uint8_t* listEntryUnderlyingData = listEntry.getUnderlyingData();
		bool isEqual = true;
		for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
		{
			if ( underlyingData[byte] != listEntryUnderlyingData[byte] )
			{
				isEqual = false;

				break;
			}
		}

		if ( isEqual )
		{
			foundEntry = true;

			break;
		}
	}

	if ( ! foundEntry ) return false;

	if ( ! entry.isRootDirectory()
			&& ! entry.isSubdirectory()
			&& ! entry.isUnusedEntry()
			&& ! entry.isDeletedEntry()
			&& ! entry.isHiddenEntry()
			&& ! entry.isSystemFile()
			&& ! entry.isDiskVolumeLabel() )
	{
		entry.getFileTransferInProgressFlagRef() = true;
		entry.getCurrentFileSectorRef() = 0;
		entry.getCurrentFileClusterRef() = entry.getStartingClusterNum();
		entry.getCurrentDirOffsetRef() = m_CurrentDirOffset;

		// move offset to first cluster of the file
		entry.getCurrentFileOffsetRef() = m_DataOffset + ( (entry.getCurrentFileClusterRef() - 2) *
							m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

		return true;
	}

	return false;
}

SharedData<uint8_t> Fat16FileManager::getSelectedFileNextSector (Fat16Entry& entry)
{
	bool& fileTransferInProgress = entry.getFileTransferInProgressFlagRef();
	unsigned int& currentFileSector = entry.getCurrentFileSectorRef();
	unsigned int& currentFileCluster = entry.getCurrentFileClusterRef();
	unsigned int& currentDirOffset = entry.getCurrentDirOffsetRef();
	unsigned int& currentFileOffset = entry.getCurrentFileOffsetRef();

	if ( fileTransferInProgress )
	{
		unsigned int returnOffset = currentFileOffset;

		currentFileSector++;
		if ( currentFileSector == m_ActiveBootSector->getNumSectorsPerCluster() )
		{
			currentFileSector = 0;

			// look up the next cluster in the FAT
			uint8_t* nextClusterByte1 = &m_FatCached[sizeof(uint16_t) * currentFileCluster];
			uint8_t* nextClusterByte2 = &m_FatCached[sizeof(uint16_t) * currentFileCluster + 1];
			uint16_t nextCluster = *nextClusterByte1 | ( *nextClusterByte2 << 8 );

			currentFileCluster = nextCluster;

			if ( currentFileCluster == FAT16_FREE_CLUSTER
					|| currentFileCluster == FAT16_END_OF_FILE_CLUSTER
					|| currentFileCluster == FAT16_BAD_CLUSTER
					|| currentFileCluster == FAT16_RESERVED_CLUSTER )
			{
				this->endFileTransfer( entry );
			}
		}

		currentFileOffset = m_DataOffset + ( (currentFileCluster - 2) *
					m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() ) +
					( currentFileSector * m_ActiveBootSector->getSectorSizeInBytes() );

		return m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(), returnOffset );
	}

	return SharedData<uint8_t>::MakeSharedDataNull();
}

void Fat16FileManager::changePartition (unsigned int partitionNum)
{
	if ( ! m_PartitionTables.empty() )
	{
		IFatFileManager::changePartition( partitionNum );

		m_RootDirectoryOffset = ( m_PartitionTables.at(m_ActivePartitionNum).getOffsetLBA() + m_ActiveBootSector->getNumReservedSectors() +
						m_ActiveBootSector->getNumFats() * m_ActiveBootSector->getNumSectorsPerFat() ) *
						m_ActiveBootSector->getSectorSizeInBytes();

		m_CurrentDirectoryEntries.clear();

		m_CurrentDirOffset = m_RootDirectoryOffset;

		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( FAT16_ENTRY_SIZE * m_ActiveBootSector->getNumDirectoryEntriesInRoot(),
									m_RootDirectoryOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			m_CurrentDirectoryEntries.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
}

bool Fat16FileManager::createEntry (Fat16Entry& entry)
{
	this->endFileTransfer( entry );

	// look for a starting cluster number (first two are reserved)
	unsigned int numClustersInFat = ( m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes() ) / 2;
	for ( unsigned int clusterNum = 2; clusterNum < numClustersInFat - 2; clusterNum++ )
	{
		uint8_t* clusterValByte1 = &m_FatCached[sizeof(uint16_t) * clusterNum];
		uint8_t* clusterValByte2 = &m_FatCached[sizeof(uint16_t) * clusterNum + 1];
		uint16_t clusterVal = *clusterValByte1 | ( *clusterValByte2 << 8 );

		if ( clusterVal == FAT16_FREE_CLUSTER )
		{
			// set cluster to end of file cluster
			std::vector<Fat16ClusterMod>& clustersToModify = entry.getClustersToModifyRef();
			Fat16ClusterMod clusterMod = { static_cast<uint16_t>(clusterNum), FAT16_END_OF_FILE_CLUSTER };
			clustersToModify.push_back( clusterMod );

			// set initial entry values
			entry.getFileTransferInProgressFlagRef() = true;
			entry.getCurrentFileSectorRef() = 0;
			entry.getCurrentFileClusterRef() = clusterNum;
			entry.getCurrentDirOffsetRef() = m_CurrentDirOffset;
			entry.getCurrentFileOffsetRef() = m_DataOffset + ( (entry.getCurrentFileClusterRef() - 2) *
							m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );
			entry.setStartingClusterNum( clusterNum );
			entry.setFileSizeInBytes( 0 );

			return true;
		}
	}

	return false;
}

bool Fat16FileManager::writeToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data)
{
	bool& fileTransferInProgress = entry.getFileTransferInProgressFlagRef();
	unsigned int& currentFileSector = entry.getCurrentFileSectorRef();
	unsigned int& currentFileCluster = entry.getCurrentFileClusterRef();
	unsigned int& currentFileOffset = entry.getCurrentFileOffsetRef();

	SharedData<uint8_t> dataToWrite = SharedData<uint8_t>::MakeSharedData( m_ActiveBootSector->getSectorSizeInBytes() );
	unsigned int totalBytesToWrite = data.getSizeInBytes();
	unsigned int bytesWritten = 0;

	while ( fileTransferInProgress && bytesWritten < totalBytesToWrite )
	{
		// first write data up to sector size of bytes
		unsigned int bytesLeftToWrite = totalBytesToWrite - bytesWritten;
		unsigned int writeToNumBytes = std::min( static_cast<unsigned int>(m_ActiveBootSector->getSectorSizeInBytes()), bytesLeftToWrite );
		for ( unsigned int byte = 0; byte < writeToNumBytes; byte++ )
		{
			dataToWrite[byte] = data[bytesWritten];
			bytesWritten++;
		}

		unsigned int writeOffset = currentFileOffset;

		currentFileSector++;
		if ( currentFileSector == m_ActiveBootSector->getNumSectorsPerCluster() )
		{
			currentFileSector = 0;

			// look for next free cluster (first two are reserved)
			bool foundFreeCluster = false;
			unsigned int numClustersInFat = (m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes()) / 2;
			for ( unsigned int clusterNum = 2; clusterNum < numClustersInFat - 2; clusterNum++ )
			{
				uint8_t* clusterValByte1 = &m_FatCached[sizeof(uint16_t) * clusterNum];
				uint8_t* clusterValByte2 = &m_FatCached[sizeof(uint16_t) * clusterNum + 1];
				uint16_t clusterVal = *clusterValByte1 | ( *clusterValByte2 << 8 );

				if ( clusterVal == FAT16_FREE_CLUSTER )
				{
					std::vector<Fat16ClusterMod>& clusterModVec = entry.getClustersToModifyRef();

					// set old cluster to new free cluster
					Fat16ClusterMod& oldClusterMod = clusterModVec.back();
					oldClusterMod.clusterNewVal = clusterVal;

					// set new cluster to end of file
					Fat16ClusterMod newClusterMod = { clusterVal, FAT16_END_OF_FILE_CLUSTER };
					clusterModVec.push_back( newClusterMod );

					// set entry values
					currentFileSector = 0;
					currentFileCluster = clusterVal;
					currentFileOffset = m_DataOffset + ( (currentFileCluster - 2) *
							m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

					foundFreeCluster = true;;
				}
			}

			if ( ! foundFreeCluster )
			{
				fileTransferInProgress = false;

				return false;
			}
		}

		currentFileOffset = m_DataOffset + ( (currentFileCluster - 2) *
					m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() ) +
					( currentFileSector * m_ActiveBootSector->getSectorSizeInBytes() );

		unsigned int oldFileSize = entry.getFileSizeInBytes();
		entry.setFileSizeInBytes( oldFileSize + writeToNumBytes );

		m_StorageMedia.writeToMedia( data, writeOffset );
	}

	return true;
}

bool Fat16FileManager::finalizeEntry (Fat16Entry& entry)
{
	const unsigned int& entryDirOffset = entry.getCurrentDirOffsetRef();
	std::vector<Fat16Entry> entriesInDir;
	bool entryIsInCurrentDirectory = false;

	// load directory entries in other directory if necessary
	if ( entryDirOffset != m_CurrentDirOffset )
	{
		SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(), entryDirOffset );
		// fill directory entries vector
		uint8_t* entriesPtr = entries.getPtr();
		for ( unsigned int entry = 0; entry < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entry++ )
		{
			entriesInDir.push_back( Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
	else // else just use current directory entries
	{
		entriesInDir = m_CurrentDirectoryEntries;
		entryIsInCurrentDirectory = true;
	}

	// find an unused entry to write the new entry to
	Fat16Entry* entryToModify = nullptr;
	unsigned int entryToModifyNum = 0;
	for ( unsigned int entryNum = 0; entryNum < m_ActiveBootSector->getNumDirectoryEntriesInRoot(); entryNum++ )
	{
		Fat16Entry& entry( entriesInDir.at(entryNum) );
		if ( entry.isUnusedEntry() || entry.isDeletedEntry() )
		{
			entryToModify = &entry;
			entryToModifyNum = entryNum;

			break;
		}
	}

	if ( entryToModify == nullptr ) return false;

	*entryToModify = entry;

	// write entry back
	SharedData<uint8_t> entryData = SharedData<uint8_t>::MakeSharedData( FAT16_ENTRY_SIZE );
	unsigned int bytesWritten = 0;
	const uint8_t* underlyingData = entryToModify->getUnderlyingData();
	for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
	{
		entryData[bytesWritten] = underlyingData[byte];
		bytesWritten++;
	}

	m_StorageMedia.writeToMedia( entryData, entryDirOffset + (entryToModifyNum * FAT16_ENTRY_SIZE) );

	// apply changes to fat
	for ( const Fat16ClusterMod& clusterMod : entry.getClustersToModifyRef() )
	{
		uint8_t* clusterValByte1 = &m_FatCached[sizeof(uint16_t) * clusterMod.clusterNum];
		uint8_t* clusterValByte2 = &m_FatCached[sizeof(uint16_t) * clusterMod.clusterNum + 1];
		*clusterValByte1 = ( clusterMod.clusterNewVal & 0x00FF );
		*clusterValByte2 = ( clusterMod.clusterNewVal & 0xFF00 ) >> 8;
	}

	// write fats back (second for redundancy)
	m_StorageMedia.writeToMedia( m_FatCached, m_FatOffset );
	m_StorageMedia.writeToMedia( m_FatCached, m_FatOffset
					+ (m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes()) );

	this->endFileTransfer( entry );
	this->endFileTransfer( *entryToModify );

	// write change to cached current directory entries if file exists in the current directory
	if ( entryIsInCurrentDirectory )
	{
		m_CurrentDirectoryEntries.at( entryToModifyNum ) = *entryToModify;
	}

	return true;
}

void Fat16FileManager::endFileTransfer (Fat16Entry& entry)
{
	// clear any clusters that were to be modified in the fat and end any previous write or read process
	entry.getFileTransferInProgressFlagRef() = false;
	entry.getCurrentFileSectorRef() = 0;
	entry.getCurrentFileClusterRef() = 0;
	entry.getCurrentDirOffsetRef() = 0;
	entry.getCurrentFileOffsetRef() = 0;
	entry.getClustersToModifyRef().clear();
}
