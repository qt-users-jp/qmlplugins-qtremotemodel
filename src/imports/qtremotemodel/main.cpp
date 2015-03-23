/* Copyright (c) 2015 Tasuku Suzuki.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Tasuku Suzuki nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL TASUKU SUZUKI BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlParserStatus>
#include <QtQml/qqml.h>

#include <QtRemoteModel/QRemoteModelServer>
#include <QtRemoteModel/QRemoteModelClient>

class RemoteModelServer : public QRemoteModelServer, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(QVariant model MEMBER model NOTIFY modelChanged)
    Q_PROPERTY(SpecialAddress address MEMBER address NOTIFY addressChanged)
    Q_PROPERTY(int port MEMBER port NOTIFY portChanged)
    Q_ENUMS(SpecialAddress)
    Q_INTERFACES(QQmlParserStatus)
public:
    enum SpecialAddress {
        Null = QHostAddress::Null,
        Broadcast = QHostAddress::Broadcast,
        LocalHost = QHostAddress::LocalHost,
        LocalHostIPv6 = QHostAddress::LocalHostIPv6,
        Any = QHostAddress::Any,
        AnyIPv6 = QHostAddress::AnyIPv6,
        AnyIPv4 = QHostAddress::AnyIPv4
    };
    RemoteModelServer(QObject *parent = 0)
        : QRemoteModelServer(parent)
        , address(LocalHost)
        , port(7174)
    {}
    void classBegin() {}
    void componentComplete() {
        QAbstractItemModel *m = qvariant_cast<QAbstractItemModel *>(model);
        setModel(m);
        listen(static_cast<QHostAddress::SpecialAddress>(address), port);
    }

signals:
    void modelChanged(const QVariant &model);
    void addressChanged(SpecialAddress address);
    void portChanged(int port);

private:
    QVariant model;
    SpecialAddress address;
    int port;
};

class RemoteModelClient : public QRemoteModelClient, public QQmlParserStatus
{
    Q_OBJECT
    Q_PROPERTY(SpecialAddress address MEMBER address NOTIFY addressChanged)
    Q_PROPERTY(int port MEMBER port NOTIFY portChanged)
    Q_ENUMS(SpecialAddress)
    Q_INTERFACES(QQmlParserStatus)
public:
    enum SpecialAddress {
        Null = QHostAddress::Null,
        Broadcast = QHostAddress::Broadcast,
        LocalHost = QHostAddress::LocalHost,
        LocalHostIPv6 = QHostAddress::LocalHostIPv6,
        Any = QHostAddress::Any,
        AnyIPv6 = QHostAddress::AnyIPv6,
        AnyIPv4 = QHostAddress::AnyIPv4
    };
    RemoteModelClient(QObject *parent = 0)
        : QRemoteModelClient(parent)
        , address(LocalHost)
        , port(7174)
    {}
    void classBegin() {}
    void componentComplete() {
        connectToHost(static_cast<QHostAddress::SpecialAddress>(address), port);
    }

signals:
    void addressChanged(SpecialAddress address);
    void portChanged(int port);

private:
    SpecialAddress address;
    int port;
};

class QmlRemoteModel : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")
public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtRemoteModel"));
        // @uri QtRemoteModel
        qmlRegisterType<RemoteModelServer>(uri, 0, 1, "RemoteModelServer");
        qmlRegisterType<RemoteModelClient>(uri, 0, 1, "RemoteModelClient");
    }
};

#include "main.moc"
