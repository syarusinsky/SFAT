#ifndef FAT16FILEMANAGER_HPP
#define FAT16FILEMANAGER_HPP

/**************************************************************************
 * The Fat16FileManager class defines a tool for navigating through
 * a FAT16 file system and retrieving and writing data.
**************************************************************************/

#include "IFatFileManager.hpp"
#include "Fat16Entry.hpp"

#include <set>

class Fat16FileManager : public IFatFileManager
{
	public:
		Fat16FileManager (IStorageMedia& storageMedia);
		~Fat16FileManager() override;

		// This function places the 'cursor' on a given directory and returns the selected entry. If a subdirectory is selected
		// the current directory entries are updated.
		Fat16Entry selectEntry (unsigned int entryNum);

		// Returns false if file is already deleted or should not be able to be deleted, true if successful
		bool deleteEntry (unsigned int entryNum);

		// The order of file writing operations are createEntry -> writeToEntry(xHoweverManyTimes) -> finalizeEntry()
		// returns false if no space available
		bool createEntry (Fat16Entry& entry);
		// writes in multiples of sector sizes, returns false if data doesn't even fit into sectors or there is no more free space
		bool writeToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data);
		// writes data less than a sector size and finalizes the entry, returns false if there is no more free space
		bool flushToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data);
		// returns false if there are no available entries in directory, true if successful
		bool finalizeEntry(Fat16Entry& entry);

		std::vector<Fat16Entry>& getCurrentDirectoryEntries() { return m_CurrentDirectoryEntries; }

		// The order of file reading operations are readEntry -> getSelectedFileNextSector(xHoweverManyTimes)
		// returns true if entry is readable file and read process has begun, false if fail
		bool readEntry (Fat16Entry& entry);
		SharedData<uint8_t> getSelectedFileNextSector (Fat16Entry& entry);

		void changePartition (unsigned int partitionNum) override;

	private:
		unsigned int 			m_FatOffset;
		SharedData<uint8_t> 		m_FatCached;
		unsigned int 			m_RootDirectoryOffset;
		unsigned int 			m_DataOffset;
		unsigned int 			m_CurrentDirOffset;
		std::vector<Fat16Entry> 	m_CurrentDirectoryEntries;

		std::set<uint16_t> 		m_PendingClustersToModify;

		bool writeToEntry (Fat16Entry& entry, const SharedData<uint8_t>& data, bool flush);

		void endFileTransfer (Fat16Entry& entry);
};

#endif // FAT16FILEMANAGER_HPP
