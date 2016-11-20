#ifndef HOSTINFO_H
#define HOSTINFO_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QString>

namespace QTerm
{
typedef QMap<QString, QString> Completion;

class HostInfo : public QObject
{
public:
    enum Type {
        Telnet,
        SSH
    };
    HostInfo(const QString & hostName, quint16 port, QObject * parent = 0);
    HostInfo();
    virtual ~HostInfo();
    void setHostName(const QString & hostName);
    void setPort(quint16 port);
    const QString & hostName();
    quint16 port();
    Type type();
    const QString & termType();
    void setTermType(const QString & termType);
protected:
    void setType(Type type);
private:
    QString m_hostName;
    quint16 m_port;
    QString m_termType;
    Type m_type;
};

class TelnetInfo : public HostInfo
{
public:
    TelnetInfo(const QString & hostName, quint16 port, QObject * parent = 0);
    ~TelnetInfo();
};
class SSHInfo : public HostInfo
{
    Q_OBJECT
public:
    enum QueryType {
        Normal,
        Password
    };
    SSHInfo(const QString & hostName, quint16 port, QObject * parent = 0);
    SSHInfo();
    virtual ~SSHInfo();
    void setUserName(const QString & userName);
    void setPassword(const QString & password);
    void setPassphrase(const QString & passphrase);
    void setPublicKeyFile(const QString & filename);
    void setPrivateKeyFile(const QString & filename);
    void setHostKey(const QString & hostKey);
    void setAutoCompletion(const Completion & autoCompletion);
    const QString & userName(bool * ok = 0);
    const QString & password(bool * ok = 0);
    const QString & passphrase();
    const QString & publicKeyFile();
    const QString & privateKeyFile();
    bool publicKeyAuthAvailable();
    bool checkHostKey(const QByteArray & hostKey);
    const QString & answer(const QString & prompt, QueryType type = Normal, bool * ok = 0);
    void reset();

    static int passphraseCallback(char *buf, int size, int rwflag, void *u);

private:
    QString m_userName;
    QString m_password;
    QString m_publicKeyFile;
    QString m_privateKeyFile;
    QString m_passphrase;
    QString m_hostKey;
    Completion m_autoCompletion;
};

} // namespace QTerm
#endif // HOSTINFO_H
