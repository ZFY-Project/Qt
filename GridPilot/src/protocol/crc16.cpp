#include "crc16.h"

namespace {
quint16 table[256];
bool tableReady = false;
void buildTable()
{
    const quint16 compare=0xA001;
    for(int i=0;i<256;i++)
    {
        quint16 value=i;
        for(int j=0;j<8;j++)
        {
            if(value & 1)
            {
                value >>= 1;
                value ^= compare;
            }
            else
            {
                value >>= 1;
            }
        }
        table[i]=value;
    }
    tableReady=true;
}

}

Crc16::Crc16() {}

quint16 Crc16::compute(const QByteArray& data)
{
    if(!tableReady)
    {
        buildTable();
    }
    quint16 crc=0xFFFF;
    for (int i = 0; i < data.size(); ++i)
    {
        const quint8 b = static_cast<quint8>(data.at(i));  // char 转 quint8，防负数
        const quint16 idx = (crc ^ b) & 0xFF;
        crc = static_cast<quint16>((crc >> 8) ^ table[idx]);
    }

    return crc;
}