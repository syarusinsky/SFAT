#ifndef IFATFILEMANAGER_HPP
#define IFATFILEMANAGER_HPP

/**************************************************************************
 * An IFatFileManager subclass defines a tool for navigating through
 * a specific FAT (FAT12/FAT16/FAT32) file system and retrieving and
 * writing data.
**************************************************************************/

#include "IStorageMedia.hpp"
#include "BootSector.hpp"

#include <vector>

class PartitionTable;

class IFatFileManager
{
	public:
		IFatFileManager (IStorageMedia& storageMedia);
		virtual ~IFatFileManager();

		bool isValidFatFileSystem();

		virtual void changePartition (unsigned int partitionNum);

		BootSector* getActiveBootSector() { return m_ActiveBootSector; }

		std::vector<PartitionTable>* getPartitionTables() { return &m_PartitionTables; }

	protected:
		IStorageMedia& 			m_StorageMedia;
		unsigned int 			m_ActivePartitionNum;
		std::vector<PartitionTable> 	m_PartitionTables;
		BootSector* 			m_ActiveBootSector;
};

#endif // IFATFILEMANAGER_HPP
