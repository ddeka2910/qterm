//
// C++ Implementation: sshchannel
//
// Description:
//
//
// Author: hooey <hephooey@gmail.com>, (C) 2007
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "channel.h"
#include "packet.h"
#include "ssh2.h"
#include "ssh1.h"
#include <QtDebug>
extern void dumpData(const QByteArray & data);
namespace QTerm
{

SSH2Channel::SSH2Channel(SSH2InBuffer * in, SSH2OutBuffer * out, QObject *parent)
        : QObject(parent)
{
    m_in = in;
    m_out = out;
    connect(m_in, SIGNAL(packetReady(int)), this, SLOT(channelPacketReceived(int)));
}


SSH2Channel::~SSH2Channel()
{
    foreach(Channel * target, m_channelList)
    delete target;
}

void SSH2Channel::channelPacketReceived(int flag)
{
    // TODO: other flags;
    switch (flag) {
    case SSH2_MSG_CHANNEL_OPEN_CONFIRMATION:
        channelOpened();
        break;
    case SSH2_MSG_CHANNEL_OPEN_FAILURE:
        // TODO: handle failure
        qDebug() << "open channel failed";
        break;
    case SSH2_MSG_CHANNEL_WINDOW_ADJUST:
        remoteWindowAdjusted();
        break;
    case SSH2_MSG_CHANNEL_DATA:
        receiveData();
        break;
    case SSH2_MSG_CHANNEL_EOF:
        qDebug() << "channel eof";
        break;
    case SSH2_MSG_CHANNEL_CLOSE:
        qDebug() << "channel closed";
        break;
    case SSH2_MSG_CHANNEL_REQUEST:
        qDebug() << "channel request";
        m_in->getUInt8();
        qDebug() << "recipient channel " << m_in->getUInt32();
        qDebug() << "request string " << m_in->getString();
        qDebug() << "want reply " << m_in->getUInt8();
        break;
    default:
        qDebug() << "unknown packet: " << flag;
        break;
    }
}

void SSH2Channel::receiveData()
{
    m_in->getUInt8();
    Channel * target = m_channelList.at(m_in->getUInt32());
    QByteArray data = m_in->getString();
    qDebug() << "===remote ID " << target->remoteID << target->remoteWindow;
    // TODO: wait for window adjust?
    if (target->localWindow < data.size())
        qDebug() << "local window size is too small";
    else
        target->localWindow -= data.size();
    qDebug() << "local window size" << target->localWindow;
    // qDebug() << data;
    target->data += data;
    if (target->localWindow < 2*target->localPacketSize) {
        adjustWindow(target->localID, 2*target->localPacketSize);
        target->localWindow += 2 * target->localPacketSize;
    }
    emit dataReady(target->localID);
}

void SSH2Channel::writeData(int id, const QByteArray & data)
{
    if (m_channelList.at(id)->remoteWindow < data.size()) {
        qDebug() << "remote window too small";
        return;
    }
    m_channelList.at(id)->remoteWindow -= data.size();
    m_out->startPacket(SSH2_MSG_CHANNEL_DATA);
    m_out->putUInt32(id);
    m_out->putString(data);
    m_out->sendPacket();
}

void SSH2Channel::channelOpened()
{
    m_in->getUInt8();
    Channel * target = m_channelList.at(m_in->getUInt32());
    target->remoteID = m_in->getUInt32();
    target->remoteWindow = m_in->getUInt32();
    target->remotePacketSize = m_in->getUInt32();
    m_in->atEnd();
    qDebug() << "ID: " << target->localID << target->remoteID << "windows size: " << target->localWindow << target->remoteWindow << "packet size: " << target->localPacketSize << target->remotePacketSize;
    requestPty(target->localID);
}

void SSH2Channel::adjustWindow(uint id, uint size)
{
    m_out->startPacket(SSH2_MSG_CHANNEL_WINDOW_ADJUST);
    m_out->putUInt32(id);
    m_out->putUInt32(size);
    m_out->sendPacket();
}

void SSH2Channel::remoteWindowAdjusted()
{
    m_in->getUInt8();
    uint id = m_in->getUInt32();
    m_channelList.at(id)->remoteWindow += m_in->getUInt32();
    qDebug() << "remote window: " << m_channelList.at(id)->remoteWindow;
}

void SSH2Channel::requestPty(uint id)
{
    // TODO: Env
    qDebug() << "request PTY";
    m_out->startPacket(SSH2_MSG_CHANNEL_REQUEST);
    m_out->putUInt32(id);
    m_out->putString("pty-req");
    m_out->putUInt8(0);
    // TODO: "xterm" does not work somehow
    m_out->putString("vt100");
    // TODO: configuration
    m_out->putUInt32(80);
    m_out->putUInt32(24);
    m_out->putUInt32(0);
    m_out->putUInt32(0);
    m_out->putString("");
    m_out->sendPacket();
    qDebug() << "start shell";
    m_out->startPacket(SSH2_MSG_CHANNEL_REQUEST);
    m_out->putUInt32(id);
    m_out->putString("shell");
    m_out->putUInt8(0);
    m_out->sendPacket();
}

/*!
    \fn QTerm::SSH2Channel::openChannel(u_int32_t localID, SSH2InBuffer * in, SSH2OutBuffer * out)
 */
void SSH2Channel::openChannel()
{
    Channel * newChannel = new Channel;
    newChannel->localID = m_channelList.size();
    newChannel->localWindow = 4 * 32 * 1024;
    newChannel->localPacketSize = 32 * 1024;
    newChannel->data.resize(0);
    m_out->startPacket(SSH2_MSG_CHANNEL_OPEN);
    m_out->putString("session");
    m_out->putUInt32(newChannel->localID);
    m_out->putUInt32(newChannel->localWindow);
    m_out->putUInt32(newChannel->localPacketSize);
    m_out->sendPacket();
    m_channelList.append(newChannel);
}

QByteArray SSH2Channel::readData(int id, unsigned long size)
{
    Channel * target = m_channelList.at(id);
    QByteArray data = target->data.left(size);
    target->data.remove(0, data.size());
    return data;
//  m_channelList[id].data.resize(0);
}

unsigned long SSH2Channel::bytesAvailable(int id)
{
    Channel * target = m_channelList.at(id);
    return target->data.size();
}

SSH1Channel::SSH1Channel(SSH1InBuffer * in, SSH1OutBuffer * out, QObject *parent)
        : QObject(parent), m_status(RequestPty), m_data()
{
    m_in = in;
    m_out = out;
    connect(m_in, SIGNAL(packetReady(int)), this, SLOT(channelPacketReceived(int)));
    requestPty();
}

SSH1Channel::~SSH1Channel()
{}

void SSH1Channel::requestPty()
{
    m_out->startPacket(SSH_CMSG_REQUEST_PTY);
    m_out->putString("vt100");
    m_out->putUInt32(24);
    m_out->putUInt32(80);
    m_out->putUInt32(0);
    m_out->putUInt32(0);
    m_out->putUInt8(0);
    m_out->sendPacket();
    qDebug() << "Request pty";
    m_out->startPacket(SSH_CMSG_EXEC_SHELL);
    m_out->sendPacket();
}

void SSH1Channel::receiveData()
{
    m_in->getUInt8();
    QByteArray data = m_in->getString();
    m_data += data;
    emit dataReady();
}

QByteArray SSH1Channel::readData(unsigned long size)
{
    QByteArray data = m_data.left(size);
    m_data.remove(0, data.size());
    return data;
}

void SSH1Channel::writeData(const QByteArray & data)
{
    m_out->startPacket(SSH_CMSG_STDIN_DATA);
    m_out->putString(data);
    m_out->sendPacket();
}

unsigned long SSH1Channel::bytesAvailable()
{
    return m_data.size();
}
void SSH1Channel::channelPacketReceived(int flag)
{
    // TODO: other flags;
    switch (flag) {
    case SSH_SMSG_SUCCESS:
        if (m_status == RequestPty) {
            qDebug() << "Pty request succeed";
            m_status = Interactive;
        }
        break;
    case SSH_SMSG_FAILURE:
        if (m_status == RequestPty)
            qDebug() << "Pty request failed";
        break;
    case SSH_SMSG_STDOUT_DATA:
    case SSH_SMSG_STDERR_DATA:
        qDebug() << "server data available";
        receiveData();
        break;
    default:
        qDebug() << "unknown packet: " << flag;
        break;
    }
}

}

#include "channel.moc"

