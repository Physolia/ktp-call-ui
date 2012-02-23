/*
    Copyright (C) 2011 Collabora Ltd. <info@collabora.co.uk>
      @author George Kiagiadakis <george.kiagiadakis@collabora.co.uk>

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
#include "qtf.h"
#include <QGlib/Quark>
#include <TelepathyQt/Farstream/PendingChannel>
#include <telepathy-farstream/telepathy-farstream.h>

//BEGIN generated code

//Autogenerated by the QtGStreamer helper code generator

#define REGISTER_TYPE_IMPLEMENTATION(T, GTYPE) \
    namespace QGlib { \
        GetTypeImpl<T>::operator Type() { return (GTYPE); } \
    }

REGISTER_TYPE_IMPLEMENTATION(QTf::Channel,TF_TYPE_CHANNEL)

REGISTER_TYPE_IMPLEMENTATION(QTf::Content,TF_TYPE_CONTENT)

namespace QTf {
    QGlib::RefCountedObject *Channel_new(void *instance)
    {
        QTf::Channel *cppClass = new QTf::Channel;
        cppClass->m_object = instance;
        return cppClass;
    }
} //namespace QTf

namespace QTf {
    QGlib::RefCountedObject *Content_new(void *instance)
    {
        QTf::Content *cppClass = new QTf::Content;
        cppClass->m_object = instance;
        return cppClass;
    }
} //namespace QTf

namespace QTf {
namespace Private {
    void registerWrapperConstructors()
    {
        QGlib::Quark q = QGlib::Quark::fromString("QGlib__wrapper_constructor");
        QGlib::GetType<Channel>().setQuarkData(q, reinterpret_cast<void*>(&Channel_new));
        QGlib::GetType<Content>().setQuarkData(q, reinterpret_cast<void*>(&Content_new));
    }
} //namespace Private
} //namespace QTf

//END generated code


namespace QTf {

bool Channel::processBusMessage(const QGst::MessagePtr & message)
{
    return tf_channel_bus_message(object<TfChannel>(), message);
}

void init()
{
    Private::registerWrapperConstructors();
}


PendingChannel::PendingChannel(const Tp::CallChannelPtr& callChannel)
    : PendingOperation(callChannel)
{
    Tp::Farstream::PendingChannel *p = Tp::Farstream::createChannel(callChannel);
    connect(p, SIGNAL(finished(Tp::PendingOperation*)),
            SLOT(onPendingTfChannelFinished(Tp::PendingOperation*)));
}

PendingChannel::~PendingChannel()
{
}

ChannelPtr PendingChannel::channel() const
{
    return m_channel;
}

void PendingChannel::onPendingTfChannelFinished(Tp::PendingOperation *op)
{
    if (op->isError()) {
        setFinishedWithError(op->errorName(), op->errorMessage());
    } else {
        m_channel = ChannelPtr::wrap(qobject_cast<Tp::Farstream::PendingChannel*>(op)->tfChannel(), false);
        setFinished();
    }
}

} //namespace QTf

#include "qtf.moc"
