#include "qterm.h"
#include "scripthelper.h"
#include "qtermwindow.h"
#include "qtermbuffer.h"
#include "qtermscreen.h"
#include "qtermframe.h"
#include "qtermbbs.h"
#include "qtermtextline.h"
#include "qtermzmodem.h"
#include "qtermglobal.h"
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QAction>
#include <QMenu>
#include <QtScript>

namespace QTerm
{
ScriptHelper::ScriptHelper(Window * parent, QScriptEngine * engine)
    :QObject(parent),m_accepted(false),m_qtbindingsAvailable(true),m_scriptList(),m_popupActionList(),m_urlActionList()
{
    m_window = parent;
    m_scriptEngine = engine;
    m_scriptEngine->installTranslatorFunctions();
#if QT_VERSION >= 0x050000
    m_qtbindingsAvailable = false;
#else
    QStringList allowedBindings;
    allowedBindings << "qt.core" << "qt.gui" << "qt.sql" << "qt.xml" << "qt.uitools" << "qt.network" << "qt.webkit";
    foreach( QString binding, allowedBindings )
    {
        QScriptValue error = engine->importExtension( binding );
        if( error.isUndefined() )
        { // undefined indiciates success
            continue;
        }

        qDebug() << "Extension" << binding <<  "not found:" << error.toString();
        qDebug() << "Available extensions:" << engine->availableExtensions();
        qDebug() << "Some script functions will be disabled, considering install QtScriptBindings!";
        m_qtbindingsAvailable = false;
    }
    if (m_qtbindingsAvailable)
        qDebug() << "QtScriptBindings loaded, enjoy scripting!";
#endif
}

ScriptHelper::~ScriptHelper()
{
}

bool ScriptHelper::accepted() const
{
    return m_accepted;
}

bool ScriptHelper::qtbindingsAvailable() const
{
    return m_qtbindingsAvailable;
}

void ScriptHelper::setAccepted(bool accepted)
{
    m_accepted =  accepted;
}

int ScriptHelper::caretX()
{
    return m_window->m_pBuffer->caret().x();
}

int ScriptHelper::caretY()
{
    return m_window->m_pBuffer->caret().y();
}

int ScriptHelper::charX(int x, int y)
{
    QPoint pt = m_window->screen()->mapToChar(QPoint(x,y));
    return pt.x();
}

int ScriptHelper::charY(int x, int y)
{
    QPoint pt = m_window->screen()->mapToChar(QPoint(x,y));
    return pt.y() - m_window->m_pBBS->getScreenStart();
}

int ScriptHelper::posX()
{
    return m_window->mousePos().x();
}

int ScriptHelper::posY()
{
    return m_window->mousePos().y();
}

int ScriptHelper::columns()
{
    return m_window->m_pBuffer->columns();
}

int ScriptHelper::rows()
{
    return m_window->m_pBuffer->line();
}

QString ScriptHelper::getUrl()
{
    return m_window->m_pBBS->getUrl();
}

QString ScriptHelper::getIP()
{
    return m_window->m_pBBS->getIP();
}

bool ScriptHelper::isConnected()
{
    return m_window->isConnected();
}

void ScriptHelper::reconnect()
{
    m_window->reconnect();
}

void ScriptHelper::disconnect()
{
    m_window->disconnect();
}

void ScriptHelper::buzz()
{
    m_window->m_pFrame->buzz();
}

void ScriptHelper::sendString(const QString & string)
{
    m_window->inputHandle(string);
}

void ScriptHelper::sendParsedString(const QString & string)
{
    m_window->sendParsedString(string);
}

void ScriptHelper::osdMessage(const QString & message, int type, int duration)
{
    m_window->osdMessage(message, type, duration);
}

void ScriptHelper::showMessage(const QString & title, const QString & message, int duration)
{
    m_window->showMessage(title, message, duration);
}

void ScriptHelper::cancelZmodem()
{
    m_window->zmodem()->zmodemCancel();
}

void ScriptHelper::setZmodemFileList(const QStringList & fileList)
{
    m_window->zmodem()->setFileList(fileList);
}

QScriptValue ScriptHelper::getLine(int line)
{
    TextLine * obj = m_window->m_pBuffer->screen(line);
    return m_scriptEngine->newQObject(obj);
}

QScriptValue ScriptHelper::window()
{
    return m_scriptEngine->newQObject(m_window);
}

bool ScriptHelper::addPopupMenu(QString id, QString menuTitle, QString icon)
{
    if (m_popupActionList.contains(id)) {
        qDebug("action with the same name is already registered");
        return false;
    }
    QAction * action = new QAction(menuTitle, this);
    action->setObjectName(id);
    QMenu * popupMenu = m_window->popupMenu();
    popupMenu->addAction(action);
    QScriptValue newItem = m_scriptEngine->newQObject( action );
    m_scriptEngine->globalObject().property( "QTerm" ).setProperty( id, newItem );
    m_popupActionList << id;
    return true;
}

bool ScriptHelper::addUrlMenu(QString id, QString menuTitle, QString icon)
{
    if (m_urlActionList.contains(id)) {
        qDebug("action with the same name is already registered");
        return false;
    }
    QAction * action = new QAction(menuTitle, this);
    action->setObjectName(id);
    QMenu * urlMenu = m_window->urlMenu();
    urlMenu->addAction(action);
    QScriptValue newItem = m_scriptEngine->newQObject( action );
    m_scriptEngine->globalObject().property( "QTerm" ).setProperty( id, newItem );
    m_urlActionList << id;
    return true;
}

void ScriptHelper::addPopupSeparator()
{
    m_window->popupMenu()->addSeparator();
}

void ScriptHelper::addUrlSeparator()
{
    m_window->urlMenu()->addSeparator();
}

void ScriptHelper::addImportedScript(const QString & filename)
{
    m_scriptList << filename;
}

bool ScriptHelper::isScriptLoaded(const QString & filename)
{
    return m_scriptList.contains(filename);
}

QString ScriptHelper::globalPath()
{
    return Global::instance()->pathLib();
}

QString ScriptHelper::localPath()
{
    return Global::instance()->pathCfg();
}

QString ScriptHelper::findFile(const QString & filename)
{
    QFileInfo fileInfo(filename);
    if (!fileInfo.isAbsolute()) {
        fileInfo = QFileInfo(localPath() + "scripts/" + filename);
        if (!fileInfo.exists()) {
            fileInfo = QFileInfo(globalPath() + "scripts/" + filename);
        }
    }
    if (!fileInfo.exists()) {
        qDebug() << "Script file " << filename << "not found";
        return "";
    }
    return fileInfo.absoluteFilePath();
}

void ScriptHelper::loadScript(const QString & filename)
{
    QString scriptFile = findFile(filename);
    if (filename.isEmpty()) {
        return;
    }
    if (isScriptLoaded(scriptFile)) {
        qDebug() << "Script file " << scriptFile << "is already loaded";
        return;
    }
    loadScriptFile(scriptFile);
    qDebug() << "load script file: " << filename;
    addImportedScript(scriptFile);
}

bool ScriptHelper::loadExtension(const QString & extension)
{
    QScriptValue ret = m_scriptEngine->importExtension(extension);
    if (ret.isError()) {
        osdMessage("Fail to load extension: "+extension);
        qDebug() << "Fail to load extension: " << extension;
        return false;
    }
    return true;
}

void ScriptHelper::openUrl(const QString & url)
{
    Global::instance()->openUrl(url);
}

QString ScriptHelper::getSelectedText(bool rect, bool color, const QString & escape)
{
    return m_window->m_pBuffer->getSelectText(rect,color,escape);
}

QString ScriptHelper::version()
{
    return QTERM_VERSION;
}

void ScriptHelper::loadScriptFile(const QString & filename)
{
    QFile file(filename);
    file.open(QIODevice::ReadOnly);
    QString scripts = QString::fromUtf8(file.readAll());
    file.close();
    if (!m_scriptEngine->canEvaluate(scripts))
        qDebug() << "Cannot evaluate this script";
    m_scriptEngine->evaluate(scripts, filename);
    if (m_scriptEngine->hasUncaughtException()) {
        qDebug() << "Exception: " << m_scriptEngine->uncaughtExceptionBacktrace();
    }
}


} // namespace QTerm
#include <moc_scripthelper.cpp>
