#include "framecodec.h"
#include"crc16.h"

namespace Protocol {

FrameCodec::FrameCodec()
{

}

QByteArray FrameCodec::encode(quint16 cmd, quint16 seq, const QByteArray& payload)
{
    QByteArray frame;

    //帧头
    frame.append(static_cast<char>(0xAA));
    frame.append(static_cast<char>(0x55));
    // 长度（载荷字节数）
    frame.append(static_cast<char>(payload.size()));
    //命令字
    frame.append(static_cast<char>(cmd));
    //序号
    frame.append(static_cast<char>((seq >> 8) & 0xFF));
    frame.append(static_cast<char>(seq & 0xFF));
    //载荷
    frame.append(payload);

    // CRC：计算范围 = LEN 起至载荷末尾
    QByteArray crcSource;
    crcSource.append(static_cast<char>(payload.size()));
    crcSource.append(static_cast<char>(cmd));
    crcSource.append(static_cast<char>((seq >> 8) & 0xFF));
    crcSource.append(static_cast<char>(seq & 0xFF));
    crcSource.append(payload);

    const quint16 crc = Crc16::compute(crcSource);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));   // 高字节在前
    frame.append(static_cast<char>(crc & 0xFF));

    return frame;
}

}   //namespace Protocol