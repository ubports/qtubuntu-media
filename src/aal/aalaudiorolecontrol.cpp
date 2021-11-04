/*
 * Copyright (C) 2016 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aalaudiorolecontrol.h"

#include <QDebug>

namespace media = lomiri::MediaHub;

AalAudioRoleControl::AalAudioRoleControl
    (const std::shared_ptr<lomiri::MediaHub::Player>& playerSession)
    : QAudioRoleControl()
    , m_audioRole(QAudio::MusicRole)
    , m_hubPlayerSession(playerSession)
{
}

QAudio::Role AalAudioRoleControl::audioRole() const
{
    return m_audioRole;
}

void AalAudioRoleControl::setAudioRole(QAudio::Role role)
{
    if (m_hubPlayerSession == nullptr)
    {
        qWarning() << "Failed to setAudioRole since m_hubPlayerSession is NULL";
        return;
    }

    m_hubPlayerSession->setAudioStreamRole(fromQAudioRole(role));

    if (role != m_audioRole)
        Q_EMIT audioRoleChanged(m_audioRole = role);
}

QList<QAudio::Role> AalAudioRoleControl::supportedAudioRoles() const
{
    return QList<QAudio::Role>() << QAudio::MusicRole
                                 << QAudio::VideoRole
                                 << QAudio::AlarmRole
                                 << QAudio::NotificationRole
                                 << QAudio::RingtoneRole
                                 << QAudio::VoiceCommunicationRole;
}

QAudio::Role AalAudioRoleControl::toQAudioRole(const media::Player::AudioStreamRole &role)
{
    switch (role)
    {
        case media::Player::AudioStreamRole::MultimediaRole:
            return QAudio::MusicRole;
        case media::Player::AudioStreamRole::AlarmRole:
            return QAudio::AlarmRole;
        case media::Player::AudioStreamRole::AlertRole:
            return QAudio::NotificationRole;
        case media::Player::AudioStreamRole::PhoneRole:
            return QAudio::VoiceCommunicationRole;
        default:
            qWarning() << "Unhandled or invalid lomiri::MediaHub::AudioStreamRole: " << role;
            return QAudio::MusicRole;
    }
}

media::Player::AudioStreamRole AalAudioRoleControl::fromQAudioRole(const QAudio::Role &role)
{
    // If we don't have a valid role, this should be the default translation
    if (role == QAudio::Role::UnknownRole)
        return media::Player::AudioStreamRole::MultimediaRole;

    switch (role)
    {
        case QAudio::MusicRole:
        case QAudio::VideoRole:
            return media::Player::AudioStreamRole::MultimediaRole;
        case QAudio::AlarmRole:
            return media::Player::AudioStreamRole::AlarmRole;
        case QAudio::NotificationRole:
        case QAudio::RingtoneRole:
            return media::Player::AudioStreamRole::AlertRole;
        case QAudio::VoiceCommunicationRole:
            return media::Player::AudioStreamRole::PhoneRole;
        default:
            qWarning() << "Unhandled or invalid QAudio::Role:" << role;
            return media::Player::AudioStreamRole::MultimediaRole;
    }
}
