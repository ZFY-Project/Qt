#ifndef CRC16_H
#define CRC16_H

#include<QByteArray>
#include<QtGlobal>

class Crc16
{
public:
    Crc16();
    static quint16 compute(const QByteArray& data);
};

#endif // CRC16_H
