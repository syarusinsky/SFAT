#include "PartitionTable.hpp"

PartitionTable::PartitionTable (uint32_t* offset) :
	m_Bootable( 0 ),
	m_StartAddrCHS( 0 ),
	m_PartitionType( 0 ),
	m_EndAddrCHS( 0 ),
	m_OffsetLBA( 0 ),
	m_PartitionSize( 0 )
{
	uint8_t* data = reinterpret_cast<uint8_t*>( offset );

	m_Bootable = *data;
	m_StartAddrCHS = ( *(data + 1) ) | ( *(data + 2) << 8 ) | ( *(data + 3) << 16 );
	m_PartitionType = *( data + 4 );
	m_EndAddrCHS = ( *(data + 5) ) | ( *(data + 6) << 8 ) | ( *(data + 7) << 16 );
	m_OffsetLBA = ( *(data + 8) ) | ( *(data + 9) << 8 ) | ( *(data + 10) << 16 ) | ( *(data + 11) << 24 );
	m_PartitionSize = ( *(data + 12) ) | ( *(data + 13) << 8 ) | ( *(data + 14) << 16 ) | ( *(data + 15) << 24 );
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
