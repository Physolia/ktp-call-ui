/*
    Copyright (C) 2009  George Kiagiadakis <kiagiadakis.george@gmail.com>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "accountmanager.h"
#include "accountitem.h"
#include <QtCore/QHash>
#include <KDebug>
#include <TelepathyQt4/AccountManager>
#include <TelepathyQt4/PendingReady>

struct AccountManager::Private
{
    Private(AccountManager *qq) : q(qq) {}

    void init();
    void updateOnlineStatus();
    void setGlobalConnectionStatus(bool online);

    //Q_PRIVATE_SLOTs
    void onAccountManagerReady(Tp::PendingOperation *op);
    void onAccountCreated(const QString & path);
    void onAccountReady(Tp::PendingOperation *op);
    void onAccountRemoved(const QString & path);
    void onAccountValidityChanged(const QString & path, bool isValid);
    void onAccountConnectionStatusChanged(Tp::ConnectionStatus, Tp::ConnectionStatusReason);

    Tp::AccountManagerPtr m_accountManager;
    TreeModel *m_treeModel;
    QHash<QString, AccountItem*> m_accountItemsMap;
    QHash<QString, Tp::ConnectionStatus> m_accountConnectionStatusMap;
    Tp::ConnectionStatus m_globalStatus;
    AccountManager *const q;
};

void AccountManager::Private::init()
{
    m_accountManager = Tp::AccountManager::create();
    connect(m_accountManager->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)),
            q, SLOT(onAccountManagerReady(Tp::PendingOperation*)));

    m_treeModel = new TreeModel(q);
    m_globalStatus = Tp::ConnectionStatusDisconnected;
}

void AccountManager::Private::updateOnlineStatus()
{
    //if at least one account has a connection, we are online
    Tp::ConnectionStatus newStatus = Tp::ConnectionStatusDisconnected;
    QHashIterator<QString, Tp::ConnectionStatus> it(m_accountConnectionStatusMap);

    while (it.hasNext()) {
        it.next();
        //Tp::ConnectionStatusConnected == 0, Connecting == 1 and Disconnected == 2
        //so, this updates the newStatus value only if it is "better" than the previous one
        if ( it.value() < newStatus ) {
            newStatus = it.value();
        }
    }

    if ( newStatus != m_globalStatus ) {
        m_globalStatus = newStatus;
        emit q->globalConnectionStatusChanged(newStatus);
    }
}

void AccountManager::Private::setGlobalConnectionStatus(bool online)
{
    QHashIterator<QString, AccountItem*> it(m_accountItemsMap);

    while (it.hasNext()) {
        it.next();
        Tp::AccountPtr account = it.value()->account();
        if ( account->isEnabled() ) {
            if ( online ) {
                account->setRequestedPresence(account->automaticPresence());
            } else {
                Tp::SimplePresence presence;
                presence.type = Tp::ConnectionPresenceTypeOffline;
                presence.status = "offline";
                account->setRequestedPresence(presence);
            }
        }
    }
}

void AccountManager::Private::onAccountManagerReady(Tp::PendingOperation *op)
{
    if ( op->isError() ) {
        kError() << "Account manager failed to become ready:" << op->errorMessage();
        return; //TODO handle this error
    }

    foreach(const QString & path, m_accountManager->validAccountPaths()) {
        onAccountCreated(path);
    }

    connect(m_accountManager.data(), SIGNAL(accountCreated(QString)),
            q, SLOT(onAccountCreated(QString)));
    connect(m_accountManager.data(), SIGNAL(accountRemoved(QString)),
            q, SLOT(onAccountRemoved(QString)));
    connect(m_accountManager.data(), SIGNAL(accountValidityChanged(QString, bool)),
            q, SLOT(onAccountValidityChanged(QString, bool)));
}

void AccountManager::Private::onAccountCreated(const QString & path)
{
    Tp::AccountPtr account = m_accountManager->accountForPath(path);
    Q_ASSERT(!account.isNull());

    account->ref(); //increment reference temporarily to avoid destroying the object in this block
    connect(account->becomeReady(), SIGNAL(finished(Tp::PendingOperation*)),
            q, SLOT(onAccountReady(Tp::PendingOperation*)));
}

void AccountManager::Private::onAccountReady(Tp::PendingOperation *op)
{
    Tp::PendingReady *pendingReady = qobject_cast<Tp::PendingReady*>(op);
    Q_ASSERT(pendingReady);
    Tp::Account *accountPtr = qobject_cast<Tp::Account*>(pendingReady->object());
    Q_ASSERT(accountPtr);
    Tp::AccountPtr account(accountPtr);
    account->deref(); //remove extra reference added in onAccountCreated()

    if ( op->isError() ) {
        kError() << "Account failed to become ready" << op->errorName() << op->errorMessage();
        return; //TODO and then what?
    }

    //add the account in the model. This keeps the Tp::AccountPtr internally.
    AccountItem *item = new AccountItem(account, m_treeModel->root());
    m_treeModel->root()->appendChild(item);
    m_accountItemsMap[account->objectPath()] = item; //remember the item. Used to remove the account later

    //update the global "online" status
    m_accountConnectionStatusMap[account->objectPath()] = account->connectionStatus();
    updateOnlineStatus();

    connect(account.data(),
            SIGNAL(connectionStatusChanged(Tp::ConnectionStatus,Tp::ConnectionStatusReason)), q,
            SLOT(onAccountConnectionStatusChanged(Tp::ConnectionStatus, Tp::ConnectionStatusReason)));
}

void AccountManager::Private::onAccountRemoved(const QString & path)
{
    AccountItem *item = m_accountItemsMap.take(path);
    if ( !item ) {
        return; //an invalid account was removed, which was not in our list anyway
    }

    //update the global "online" status
    int nRemoved = m_accountConnectionStatusMap.remove(path);
    Q_ASSERT(nRemoved == 1);
    updateOnlineStatus();

    m_treeModel->root()->removeChild(m_treeModel->root()->rowOfChild(item)); //NOTE item is deleted here
}

void AccountManager::Private::onAccountValidityChanged(const QString & path, bool isValid)
{
    //this is to keep invalid accounts out of our list
    if ( isValid ) {
        onAccountCreated(path);
    } else {
        onAccountRemoved(path);
    }
}

void AccountManager::Private::onAccountConnectionStatusChanged(Tp::ConnectionStatus status,
                                                               Tp::ConnectionStatusReason reason)
{
    Tp::Account *account = qobject_cast<Tp::Account*>(q->sender());
    Q_ASSERT(account);
    m_accountConnectionStatusMap[account->objectPath()] = status;
    updateOnlineStatus();

    Q_UNUSED(reason); //TODO handle the reason and show error message if necessary
}


AccountManager::AccountManager(QObject *parent)
    : QObject(parent), d(new Private(this))
{
    d->init();
}

AccountManager::~AccountManager()
{
    delete d;
}

QAbstractItemModel *AccountManager::accountsModel() const
{
    return d->m_treeModel;
}

QAbstractItemModel *AccountManager::contactsModel() const
{
    return d->m_treeModel;
}

Tp::ConnectionStatus AccountManager::globalConnectionStatus() const
{
    return d->m_globalStatus;
}

void AccountManager::connectAccounts()
{
    d->setGlobalConnectionStatus(true);
}

void AccountManager::disconnectAccounts()
{
    d->setGlobalConnectionStatus(false);
}

#include "accountmanager.moc"