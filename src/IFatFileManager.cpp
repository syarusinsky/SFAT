#include "IFatFileManager.hpp"

#include "PartitionTable.hpp"

// TODO remove this after debugging
#include <iostream>

IFatFileManager::IFatFileManager (IStorageMedia& storageMedia) :
	m_StorageMedia( storageMedia ),
	m_ActivePartitionNum( 0 ),
	m_PartitionTables(),
	m_ActiveBootSector( nullptr )
{
	// load partition tables if Master Boot Record is present on storage media
	if ( m_StorageMedia.hasMBR() )
	{
		// load all four partition tables
		SharedData<uint8_t> pt = m_StorageMedia.readFromMedia( sizeof(uint32_t) * 4, PARTITION_TABLE_OFFSET );
		uint32_t* ptBuffer = reinterpret_cast<uint32_t*>( pt.getPtr() );

		m_PartitionTables.push_back( PartitionTable(ptBuffer) );
		m_PartitionTables.push_back( PartitionTable(ptBuffer + 4) );
		m_PartitionTables.push_back( PartitionTable(ptBuffer + 8) );
		m_PartitionTables.push_back( PartitionTable(ptBuffer + 12) );

		// select first partition with valid FAT file system and load boot sector
		for ( unsigned int partition = 0; partition < 4; partition++ )
		{
			PartitionType partitionType = m_PartitionTables[partition].getPartitionType();

			if ( partitionType != PartitionType::EMPTY
					&& partitionType != PartitionType::EXTENDED_PARTITION
					&& partitionType != PartitionType::EXTENDED_LBA )
			{
				SharedData<uint8_t> bsBuffer = m_StorageMedia.readFromMedia( BOOT_SEC_SIZE_IN_BYTES,
									m_PartitionTables[partition].getOffsetLBA() * 512 );

				m_ActiveBootSector = new BootSector( bsBuffer.getPtr(), partitionType );

				m_ActivePartitionNum = partition;
			}
		}
	}
	else // if the Master Boot Record isn't preset, the boot sector is the first sector
	{
		SharedData<uint8_t> bsBuffer = m_StorageMedia.readFromMedia( BOOT_SEC_SIZE_IN_BYTES, 0 );

		m_ActiveBootSector = new BootSector( bsBuffer.getPtr() );
	}
}

IFatFileManager::~IFatFileManager()
{
	delete m_ActiveBootSector;
}

bool IFatFileManager::isValidFatFileSystem()
{
	// TODO will probably need to eventually add more checks to this

	if ( m_ActiveBootSector->bootSectorSignatureIsValid() )
	{
		return true;
	}

	return false;
}

void IFatFileManager::changePartition (unsigned int partitionNum)
{
	if ( m_StorageMedia.hasMBR() && partitionNum >= 0 && partitionNum < 4 )
	{
		PartitionType partitionType = m_PartitionTables[partitionNum].getPartitionType();

		if ( partitionType != PartitionType::EMPTY
				&& partitionType != PartitionType::EXTENDED_PARTITION
				&& partitionType != PartitionType::EXTENDED_LBA )
		{
			SharedData<uint8_t> bsBuffer = m_StorageMedia.readFromMedia( BOOT_SEC_SIZE_IN_BYTES,
								m_PartitionTables[partitionNum].getOffsetLBA() * 512 );

			delete m_ActiveBootSector;

			m_ActiveBootSector = new BootSector( bsBuffer.getPtr(), partitionType );

			m_ActivePartitionNum = partitionNum;
		}
	}
}
