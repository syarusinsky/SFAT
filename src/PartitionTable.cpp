#include "PartitionTable.hpp"

PartitionTable::PartitionTable (uint32_t* offset) :
	m_Bootable( (*offset & 0x000000FF) ),
	m_StartAddrCHS( (*offset & 0xFFFFFF00) >> 8 ),
	m_PartitionType( (*(offset + 1) & 0x000000FF) ),
	m_EndAddrCHS( (*(offset + 1) & 0xFFFFFF00) >> 8 ),
	m_OffsetLBA( *(offset + 2) ),
	m_PartitionSize( *(offset + 3) )
{
}

PartitionTable::~PartitionTable()
{
}

bool PartitionTable::isBootable() const
{
	if ( m_Bootable == 0x80 )
	{
		return true;
	}

	return false;
}

uint32_t PartitionTable::getStartAddressCHS() const
{
	return m_StartAddrCHS;
}

uint32_t PartitionTable::getEndAddressCHS() const
{
	return m_EndAddrCHS;
}

PartitionType PartitionTable::getPartitionType() const
{
	return static_cast<PartitionType>( m_PartitionType );
}

uint32_t PartitionTable::getOffsetLBA() const
{
	return m_OffsetLBA;
}

uint32_t PartitionTable::getPartitionSize() const
{
	return m_PartitionSize;
}
