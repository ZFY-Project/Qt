#ifndef FRAMECODEC_H
#define FRAMECODEC_H

#include <QByteArray>
#include <QtGlobal>

namespace Protocol{
class FrameCodec
{
public:
    FrameCodec();
    static QByteArray encode(quint16 cmd,quint16 seq,const QByteArray&payload);
};

}   // namespace Protocol

#endif // FRAMECODEC_H
