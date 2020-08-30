#ifndef PARTITION_TABLE_HPP
#define PARTITION_TABLE_HPP

/**************************************************************************
 * The PartitionTable class defines a partition entry in an MBR partition
 * block.
**************************************************************************/

#include <stdint.h>

enum class PartitionType : uint8_t
{
	EMPTY 			= 0,
	FAT12 			= 1,
	FAT16_LTOREQ_32MB 	= 4,
	EXTENDED_PARTITION 	= 5,
	FAT16_GT_32MB 		= 6,
	FAT32_LTOREQ_2GB 	= 11,
	FAT32_LBA 		= 12,
	FAT16_LBA 		= 14,
	EXTENDED_LBA 		= 15
};

class PartitionTable
{
	public:
		PartitionTable (uint32_t* offset);
		~PartitionTable();

		bool isBootable() const;

		uint32_t getStartAddressCHS() const;
		uint32_t getEndAddressCHS() const;

		PartitionType getPartitionType() const;

		uint32_t getOffsetLBA() const;

		uint32_t getPartitionSize() const;

	private:
		uint8_t  m_Bootable;
		uint32_t m_StartAddrCHS;
		uint8_t  m_PartitionType;
		uint32_t m_EndAddrCHS;
		uint32_t m_OffsetLBA;
		uint32_t m_PartitionSize;
};

#endif // PARTITION_TABLE_HPP
