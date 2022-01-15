#include "Fat16FileManager.hpp"

#include <algorithm>

#include "IAllocator.hpp"

// strictly for allocator
struct FAT_CACHED_MAX
{
	uint8_t data[122880];
};

Fat16FileManager::Fat16FileManager (IStorageMedia& storageMedia, IAllocator* fatCacheAllocator) :
	IFatFileManager( storageMedia ),
	m_Allocator( fatCacheAllocator ),
	m_FatOffset( 0 ),
	m_FatCachedSharedData( SharedData<uint8_t>::MakeSharedDataNull() ),
	m_FatCachedPtr( nullptr ),
	m_RootDirectoryOffset( 0 ),
	m_DataOffset( 0 ),
	m_CurrentDirOffset( 0 ),
	m_CurrentDirectoryEntries(),
	m_PendingClustersToModify(),
	m_WriteToEntryBuffer( SharedData<uint8_t>::MakeSharedData(this->getActiveBootSector()->getSectorSizeInBytes()) )
{
	if ( this->isValidFatFileSystem() )
	{
		unsigned int partitionOffset = 0;
		if ( ! m_PartitionTables.empty() )
		{
			partitionOffset = m_PartitionTables.at( m_ActivePartitionNum ).getOffsetLBA();
		}

		m_FatOffset = ( partitionOffset + m_ActiveBootSector->getNumReservedSectors() ) * m_ActiveBootSector->getSectorSizeInBytes();

		if ( m_Allocator )
		{
			m_FatCachedPtr = reinterpret_cast<uint8_t*>( m_Allocator->allocate<FAT_CACHED_MAX>() );

			// write each sector of the fat to the memory the allocator points to
			for ( unsigned int sector = 0; sector < m_ActiveBootSector->getNumSectorsPerFat(); sector++ )
			{
				m_FatCachedSharedData = m_StorageMedia.readFromMedia( m_ActiveBootSector->getSectorSizeInBytes(),
											(sector * m_ActiveBootSector->getSectorSizeInBytes())
											+ m_FatOffset );

				unsigned int sectorOffset = sector * m_ActiveBootSector->getSectorSizeInBytes();
				for ( unsigned int byte = 0; byte < m_ActiveBootSector->getSectorSizeInBytes(); byte++ )
				{
					m_FatCachedPtr[sectorOffset + byte] = m_FatCachedSharedData[byte];
				}
			}
		}
		else
		{
			m_FatCachedSharedData = m_StorageMedia.readFromMedia( m_ActiveBootSector->getNumSectorsPerFat()
										* m_ActiveBootSector->getSectorSizeInBytes(), m_FatOffset );
			m_FatCachedPtr = &m_FatCachedSharedData[0];
		}

		// make the current entry offset the root directory offset and load the current entry sector with root directory entries
		m_RootDirectoryOffset = m_FatOffset + ( (m_ActiveBootSector->getNumFats() * m_ActiveBootSector->getNumSectorsPerFat() ) *
						m_ActiveBootSector->getSectorSizeInBytes() );
		m_CurrentDirOffset = m_RootDirectoryOffset;

		this->writeDirectoryEntriesToVec( m_CurrentDirectoryEntries, m_RootDirectoryOffset,
							m_ActiveBootSector->getNumDirectoryEntriesInRoot() );

		m_DataOffset = m_RootDirectoryOffset + ( m_ActiveBootSector->getNumDirectoryEntriesInRoot() * FAT16_ENTRY_SIZE );
	}
}

Fat16FileManager::~Fat16FileManager()
{
}

Fat16Entry Fat16FileManager::selectEntry (unsigned int entryNum)
{
	Fat16Entry entry( *m_CurrentDirectoryEntries.at(entryNum) );

	if ( entry.isRootDirectory() )
	{
		this->freeDirectoryEntriesInVecAndClear( m_CurrentDirectoryEntries );

		m_CurrentDirOffset = m_RootDirectoryOffset;

		this->writeDirectoryEntriesToVec( m_CurrentDirectoryEntries, m_RootDirectoryOffset,
							m_ActiveBootSector->getNumDirectoryEntriesInRoot() );
	}
	else if ( entry.isSubdirectory() )
	{
		// TODO need to account for big directories where one sector isn't enough to hold them

		this->freeDirectoryEntriesInVecAndClear( m_CurrentDirectoryEntries );

		m_CurrentDirOffset = m_DataOffset + ( (entry.getStartingClusterNum() - 2) *
						m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

		this->writeDirectoryEntriesToVec( m_CurrentDirectoryEntries, m_CurrentDirOffset, m_ActiveBootSector->getSectorSizeInBytes() );
	}

	return entry;
}

bool Fat16FileManager::deleteEntry (unsigned int entryNum)
{
	if ( entryNum >= m_ActiveBootSector->getNumDirectoryEntriesInRoot() ) return false;

	Fat16Entry& entry( *m_CurrentDirectoryEntries.at(entryNum) );

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

	// to store the affected sectors numbers of the fat
	std::set<unsigned int> fatAffectedSectors;

	// set the cluster chain to unused
	uint16_t cluster = entry.getStartingClusterNum();
	while ( cluster != FAT16_END_OF_FILE_CLUSTER
			&& cluster != FAT16_BAD_CLUSTER
			&& cluster != FAT16_RESERVED_CLUSTER )
	{
		// store affected sector of fat
		fatAffectedSectors.insert( (sizeof(uint16_t) * cluster) / m_ActiveBootSector->getSectorSizeInBytes() );

		// look up the next cluster in the FAT
		uint8_t* prevClusterByte1 = &m_FatCachedPtr[sizeof(uint16_t) * cluster];
		uint8_t* prevClusterByte2 = &m_FatCachedPtr[sizeof(uint16_t) * cluster + 1];
		uint16_t prevCluster = *prevClusterByte1 | ( *prevClusterByte2 << 8 );
		cluster = prevCluster;

		// set previous cluster to free
		*prevClusterByte1 = ( FAT16_FREE_CLUSTER & 0x00FF );
		*prevClusterByte2 = ( FAT16_FREE_CLUSTER & 0xFF00 ) >> 8;
	}

	this->writeEntryToStorageMedia( entry, m_CurrentDirOffset, entryNum );

	this->writeFatsBack( fatAffectedSectors );

	return true;
}

bool Fat16FileManager::readEntry (Fat16Entry& entry)
{
	this->endFileTransfer( entry );

	// ensure this file exists in the current directory
	bool foundEntry = false;
	const uint8_t* underlyingData = entry.getUnderlyingData();
	for ( const Fat16Entry* listEntry : m_CurrentDirectoryEntries )
	{
		const uint8_t* listEntryUnderlyingData = listEntry->getUnderlyingData();
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
	unsigned int& currentFileOffset = entry.getCurrentFileOffsetRef();
	unsigned int& numBytesRead = entry.getNumBytesReadRef();

	if ( fileTransferInProgress )
	{
		unsigned int returnOffset = currentFileOffset;

		currentFileSector++;
		numBytesRead += m_ActiveBootSector->getSectorSizeInBytes();

		if ( currentFileSector == m_ActiveBootSector->getNumSectorsPerCluster() )
		{
			currentFileSector = 0;

			// look up the next cluster in the FAT
			uint8_t* nextClusterByte1 = &m_FatCachedPtr[sizeof(uint16_t) * currentFileCluster];
			uint8_t* nextClusterByte2 = &m_FatCachedPtr[sizeof(uint16_t) * currentFileCluster + 1];
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
		else if ( numBytesRead >= entry.getFileSizeInBytes() )
		{
			this->endFileTransfer( entry );
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

		this->freeDirectoryEntriesInVecAndClear( m_CurrentDirectoryEntries );

		m_CurrentDirOffset = m_RootDirectoryOffset;

		this->writeDirectoryEntriesToVec( m_CurrentDirectoryEntries, m_RootDirectoryOffset,
							m_ActiveBootSector->getNumDirectoryEntriesInRoot() );
	}
}

bool Fat16FileManager::createEntry (Fat16Entry& entry)
{
	this->endFileTransfer( entry );

	// look for a starting cluster number (first two are reserved)
	unsigned int numClustersInFat = ( m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes() ) / 2;
	for ( unsigned int clusterNum = 2; clusterNum < numClustersInFat - 2; clusterNum++ )
	{
		uint8_t* clusterValByte1 = &m_FatCachedPtr[sizeof(uint16_t) * clusterNum];
		uint8_t* clusterValByte2 = &m_FatCachedPtr[sizeof(uint16_t) * clusterNum + 1];
		uint16_t clusterVal = *clusterValByte1 | ( *clusterValByte2 << 8 );

		if ( ! m_PendingClustersToModify.count(clusterNum) && clusterVal == FAT16_FREE_CLUSTER )
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

			// add this cluster to the pending modified clusters set
			m_PendingClustersToModify.insert( clusterNum );

			return true;
		}
	}

	return false;
}

bool Fat16FileManager::writeToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data)
{
	return this->writeToEntry( entry, data, false );
}

bool Fat16FileManager::flushToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data)
{
	return this->writeToEntry( entry, data, true );
}

bool Fat16FileManager::writeToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data, bool flush)
{
	// if data doesn't fit into sector size and not currently 'flushing', return false
	bool dataDoesntFit = ( data.getSizeInBytes() % m_ActiveBootSector->getSectorSizeInBytes() != 0 ) ? true : false;
	if ( dataDoesntFit && ! flush ) return false;

	bool& fileTransferInProgress = entry.getFileTransferInProgressFlagRef();
	unsigned int& currentFileSector = entry.getCurrentFileSectorRef();
	unsigned int& currentFileCluster = entry.getCurrentFileClusterRef();
	unsigned int& currentFileOffset = entry.getCurrentFileOffsetRef();

	unsigned int totalBytesToWrite = data.getSizeInBytes();
	unsigned int bytesWritten = 0;

	while ( fileTransferInProgress && bytesWritten < totalBytesToWrite )
	{
		// first write data up to sector size of bytes
		unsigned int bytesLeftToWrite = totalBytesToWrite - bytesWritten;
		unsigned int writeToNumBytes = std::min( static_cast<unsigned int>(m_ActiveBootSector->getSectorSizeInBytes()), bytesLeftToWrite );
		for ( unsigned int byte = 0; byte < writeToNumBytes; byte++ )
		{
			m_WriteToEntryBuffer[byte] = data[bytesWritten];
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
			std::vector<Fat16ClusterMod>& clusterModVec = entry.getClustersToModifyRef();

			for ( unsigned int clusterNum = clusterModVec.back().clusterNum + 1; clusterNum < numClustersInFat - 2; clusterNum++ )
			{
				uint8_t* clusterValByte1 = &m_FatCachedPtr[sizeof(uint16_t) * clusterNum];
				uint8_t* clusterValByte2 = &m_FatCachedPtr[sizeof(uint16_t) * clusterNum + 1];
				uint16_t clusterVal = *clusterValByte1 | ( *clusterValByte2 << 8 );

				if ( ! m_PendingClustersToModify.count(clusterNum) && clusterVal == FAT16_FREE_CLUSTER )
				{

					// set old cluster to new free cluster
					Fat16ClusterMod& oldClusterMod = clusterModVec.back();
					oldClusterMod.clusterNewVal = clusterNum;

					// set new cluster to end of file
					Fat16ClusterMod newClusterMod = { static_cast<uint16_t>(clusterNum), FAT16_END_OF_FILE_CLUSTER };
					clusterModVec.push_back( newClusterMod );

					// set entry values
					currentFileSector = 0;
					currentFileCluster = clusterNum;
					currentFileOffset = m_DataOffset + ( (currentFileCluster - 2) *
							m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() );

					// add this cluster to the pending modified clusters set
					m_PendingClustersToModify.insert( clusterNum );

					foundFreeCluster = true;;

					break;
				}
			}

			if ( ! foundFreeCluster )
			{
				fileTransferInProgress = false;
				this->endFileTransfer( entry );

				return false;
			}
		}

		currentFileOffset = m_DataOffset + ( (currentFileCluster - 2) *
					m_ActiveBootSector->getNumSectorsPerCluster() * m_ActiveBootSector->getSectorSizeInBytes() ) +
					( currentFileSector * m_ActiveBootSector->getSectorSizeInBytes() );

		unsigned int oldFileSize = entry.getFileSizeInBytes();
		entry.setFileSizeInBytes( oldFileSize + writeToNumBytes );

		m_StorageMedia.writeToMedia( m_WriteToEntryBuffer, writeOffset );
	}

	if ( flush )
	{
		return this->finalizeEntry( entry );
	}

	return true;
}

bool Fat16FileManager::finalizeEntry (Fat16Entry& entry)
{
	const unsigned int& entryDirOffset = entry.getCurrentDirOffsetRef();
	std::vector<Fat16Entry*> entriesInDir;
	bool entryIsInCurrentDirectory = false;

	// load directory entries in other directory if necessary
	if ( entryDirOffset != m_CurrentDirOffset )
	{
		this->writeDirectoryEntriesToVec( entriesInDir, entryDirOffset, m_ActiveBootSector->getSectorSizeInBytes() );
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
		Fat16Entry& entry( *entriesInDir.at(entryNum) );
		if ( entry.isUnusedEntry() || entry.isDeletedEntry() )
		{
			entryToModify = &entry;
			entryToModifyNum = entryNum;

			break;
		}
	}

	if ( entryToModify == nullptr ) return false;

	*entryToModify = entry;

	this->writeEntryToStorageMedia( entry, entryDirOffset, entryToModifyNum );

	// to store the affected sectors numbers of the fat
	std::set<unsigned int> fatAffectedSectors;

	// apply changes to fat
	for ( const Fat16ClusterMod& clusterMod : entry.getClustersToModifyRef() )
	{
		// store affected sector of fat
		fatAffectedSectors.insert( (sizeof(uint16_t) * clusterMod.clusterNum) / m_ActiveBootSector->getSectorSizeInBytes() );

		uint8_t* clusterValByte1 = &m_FatCachedPtr[sizeof(uint16_t) * clusterMod.clusterNum];
		uint8_t* clusterValByte2 = &m_FatCachedPtr[sizeof(uint16_t) * clusterMod.clusterNum + 1];
		*clusterValByte1 = ( clusterMod.clusterNewVal & 0x00FF );
		*clusterValByte2 = ( clusterMod.clusterNewVal & 0xFF00 ) >> 8;
	}

	this->writeFatsBack( fatAffectedSectors );

	this->endFileTransfer( entry );

	// write change to cached current directory entries if file exists in the current directory
	if ( entryIsInCurrentDirectory )
	{
		*m_CurrentDirectoryEntries.at( entryToModifyNum ) = *entryToModify;
	}
	else // otherwise make sure entriesInDir memory is cleaned up
	{
		this->freeDirectoryEntriesInVecAndClear( entriesInDir );
	}

	return true;
}

void Fat16FileManager::endFileTransfer (Fat16Entry& entry)
{
	// clear any clusters that were to be modified in the fat and end any previous write or read process
	entry.getFileTransferInProgressFlagRef() = false;
	entry.getCurrentFileSectorRef() = 0;
	entry.getCurrentFileClusterRef() = entry.getStartingClusterNum();
	entry.getCurrentFileOffsetRef() = 0;
	entry.getNumBytesReadRef() = 0;

	// clear from pending clusters to modify
	for ( const Fat16ClusterMod& clusterMod : entry.getClustersToModifyRef() )
	{
		m_PendingClustersToModify.erase( clusterMod.clusterNum );
	}

	entry.getClustersToModifyRef().clear();
}

void Fat16FileManager::writeDirectoryEntriesToVec (std::vector<Fat16Entry*>& vec, unsigned int directoryOffset, unsigned int numDirectoryEntries)
{
	SharedData<uint8_t> entries = m_StorageMedia.readFromMedia( FAT16_ENTRY_SIZE * numDirectoryEntries, directoryOffset );

	// fill directory entries vector
	uint8_t* entriesPtr = entries.getPtr();
	if ( m_Allocator )
	{
		for ( unsigned int entry = 0; entry < numDirectoryEntries; entry++ )
		{
			vec.push_back( m_Allocator->allocate<Fat16Entry>(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
	else
	{
		for ( unsigned int entry = 0; entry < numDirectoryEntries; entry++ )
		{
			vec.push_back( new Fat16Entry(&entriesPtr[entry * FAT16_ENTRY_SIZE]) );
		}
	}
}

void Fat16FileManager::freeDirectoryEntriesInVecAndClear (std::vector<Fat16Entry*>& vec)
{
	// free directory entries and clear
	if ( m_Allocator )
	{
		for ( Fat16Entry* entryPtr : vec )
		{
			m_Allocator->free<Fat16Entry>( entryPtr );
		}
	}
	else
	{
		for ( Fat16Entry* entryPtr : vec )
		{
			delete entryPtr;
		}
	}

	vec.clear();
}

void Fat16FileManager::writeEntryToStorageMedia (const Fat16Entry& entry, unsigned int directoryOffset, unsigned int entryNum)
{
	const unsigned int offset =  directoryOffset + (entryNum * FAT16_ENTRY_SIZE);
	SharedData<uint8_t> entryData = SharedData<uint8_t>::MakeSharedData( FAT16_ENTRY_SIZE );
	const uint8_t* underlyingData = entry.getUnderlyingData();
	for ( unsigned int byte = 0; byte < FAT16_ENTRY_SIZE; byte++ )
	{
		entryData[byte] = underlyingData[byte];
	}

	m_StorageMedia.writeToMedia( entryData, offset );
}

void Fat16FileManager::writeFatsBack (std::set<unsigned int> fatAffectedSectors)
{
	// write FATs back (second for redundancy)
	unsigned int secondFatOffset = m_ActiveBootSector->getNumSectorsPerFat() * m_ActiveBootSector->getSectorSizeInBytes();
	if ( m_Allocator )
	{
		// write back each of the affected sectors to the storage media
		for ( const unsigned int affectedSector : fatAffectedSectors )
		{
			unsigned int sectorOffset = affectedSector * m_ActiveBootSector->getSectorSizeInBytes();
			for ( unsigned int byte = 0; byte < m_ActiveBootSector->getSectorSizeInBytes(); byte++ )
			{
				m_FatCachedSharedData[byte] = m_FatCachedPtr[sectorOffset + byte];
			}

			m_StorageMedia.writeToMedia( m_FatCachedSharedData, m_FatOffset + sectorOffset );
			m_StorageMedia.writeToMedia( m_FatCachedSharedData, m_FatOffset + secondFatOffset + sectorOffset );
		}
	}
	else
	{
		m_StorageMedia.writeToMedia( m_FatCachedSharedData, m_FatOffset );
		m_StorageMedia.writeToMedia( m_FatCachedSharedData, m_FatOffset + secondFatOffset );
	}
}
