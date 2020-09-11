#ifndef FAT16FILEMANAGER_HPP
#define FAT16FILEMANAGER_HPP

/**************************************************************************
 * The Fat16FileManager class defines a tool for navigating through
 * a FAT16 file system and retrieving and writing data.
**************************************************************************/

#include "IFatFileManager.hpp"
#include "Fat16Entry.hpp"

class Fat16FileManager : public IFatFileManager
{
	public:
		Fat16FileManager (IStorageMedia& storageMedia);
		~Fat16FileManager() override;

		// This function places the 'cursor' on a given file or subdirectory and returns the selected entry. If a subdirectory is selected
		// the current directory entries are updated.
		Fat16Entry selectEntry (unsigned int entryNum);

		std::vector<Fat16Entry>& getCurrentDirectoryEntries() { return m_CurrentDirectoryEntries; }

		SharedData<uint8_t> getSelectedFileNextSector();

		void changePartition (unsigned int partitionNum) override;

	private:
		unsigned int 			m_FatOffset;
		unsigned int 			m_RootDirectoryOffset;
		unsigned int 			m_DataOffset;
		unsigned int 			m_CurrentOffset;
		std::vector<Fat16Entry> 	m_CurrentDirectoryEntries;
		bool 				m_FileTransferInProgress;
		unsigned int 			m_CurrentFileSector;
		unsigned int 			m_CurrentFileCluster;
};

#endif // FAT16FILEMANAGER_HPP
