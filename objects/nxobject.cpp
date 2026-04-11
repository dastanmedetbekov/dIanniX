/*
    This file is part of IanniX, a graphical real-time open-source sequencer for digital art
    Copyright (C) 2010-2015 — IanniX Association

    Project Manager: Thierry Coduys (http://www.le-hub.org)
    Development:     Guillaume Jacquemin (https://www.buzzinglight.com)

    This file was written by Guillaume Jacquemin.

    IanniX is a free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nxobject.h"

NxObject::NxObject(ApplicationCurrent *parent, QTreeWidgetItem *ccParentItem) :
    QObject(parent), QTreeWidgetItem(ccParentItem) {
    groupId.clear();
    id = 0;
    messageTimeNowOld = 0;
    parentObject = 0;
    selectedHover = false;
    selected = false;
    hasActivity = false;
    hasActivityOld = false;
    glListRecreate = true;
    isDrag = false;
    performCollision = false;
    active = ObjectsActivityActive;
    setForeground(0, Qt::gray);
    setMessageId(0);
    lineFactor = 1;
    lineStipple = 0xFFFF;
    initialize(true);
}
void NxObject::initialize(bool firstTime) {
    if(!firstTime) {
        setGroupId("");
        setActive(1);
    }
    setSolo(0);
    setMute(0);
    setPos(NxPoint());
    setMessageTimeInterval(1);
    //setMessagePatterns("");
    setLabel("");
    setColorMultiply("255 255 255 255");
}


void NxObject::parseMessagePatternString(const QString &messagePatternsStr,
                                          QVector<QVector<QByteArray>> &outMessagePatterns,
                                          quint16 *outInterval,
                                          bool *outPerformCollision) {
    outMessagePatterns.clear();
    if(outPerformCollision)
        *outPerformCollision = false;

    QString strTemp = messagePatternsStr;
    strTemp.append(' ');

    QVector<QByteArray> messagePattern;
    QString messagePatternItem = "";
    quint16 messagePatternItemJS = 0;
    bool first = true;

    for(quint16 i = 0; i < strTemp.count(); i++) {
        QChar ch = strTemp.at(i);
        if((!messagePatternItemJS) && (ch == ' ')) {
            if(!messagePatternItem.isEmpty())
                messagePattern.append(qPrintable(messagePatternItem));
            if(outPerformCollision && messagePatternItem.contains("collision_"))
                *outPerformCollision = true;
            messagePatternItem = "";
        }
        else if((messagePatternItemJS) && (ch == '}')) {
            messagePatternItemJS--;
            if(!messagePatternItemJS) {
                if(!messagePatternItem.isEmpty()) {
                    messagePattern.append(qPrintable(QString("{" + messagePatternItem + "}")));
                    if(outPerformCollision && messagePatternItem.contains("collision_"))
                        *outPerformCollision = true;
                }
                messagePatternItem = "";
            }
            else
                messagePatternItem += ch;
        }
        else if((!messagePatternItemJS) && (ch == ',')) {
            if(!messagePatternItem.isEmpty()) {
                if(first) {
                    quint16 interval = messagePatternItem.toUInt();
                    if((interval > 0) && (outInterval))
                        *outInterval = interval;
                }
                else {
                    messagePattern.append(qPrintable(messagePatternItem));
                }
            }
            first = false;
            if(!messagePattern.isEmpty())
                outMessagePatterns.append(messagePattern);
            messagePatternItem = "";
            messagePattern.clear();
            messagePatternItemJS = 0;
        }
        else if(ch == '{') {
            if(!messagePatternItemJS)
                messagePatternItem = "";
            else
                messagePatternItem += ch;
            messagePatternItemJS++;
        }
        else {
            messagePatternItem += ch;
        }
    }

    // Handle unbalanced curly brackets
    if(!messagePatternItem.isEmpty()) {
        messagePattern.append(qPrintable(QString("{" + messagePatternItem)));
    }
    if(!messagePattern.isEmpty())
        outMessagePatterns.append(messagePattern);
}

void NxObject::setMessagePatterns(const QString &messagePatternsStr) {
    messageLabel.clear();
    parseMessagePatternString(messagePatternsStr, messagePatterns, &messageTimeInterval, &performCollision);

    foreach(const QVector<QByteArray> &messagePatternItems, messagePatterns) {
        QString messageLabelStr;
        foreach(const QByteArray &messagePatternItem, messagePatternItems)
            messageLabelStr.append(messagePatternItem + " ");
        messageLabel.append(messageLabelStr.trimmed());
    }
}

QVector<QVector<QByteArray>> NxObject::parseMessagesPattern(const QString &messagePatternsStr, quint16 *messageInterval) {
    QVector<QVector<QByteArray>> result;
    parseMessagePatternString(messagePatternsStr, result, messageInterval, nullptr);
    return result;
}

void NxObject::dispatchProperty(const char *_property, const QVariant & value) {
    QStringList asCurvePoints = QStringList() << COMMAND_CURVE_POINT_RMV << COMMAND_CURVE_TXT << COMMAND_CURVE_LINES << COMMAND_CURVE_POINT << COMMAND_CURVE_POINT_TRANSLATE << COMMAND_CURVE_POINT_SHIFT << COMMAND_CURVE_EDITOR << COMMAND_CURVE_RESAMPLE << COMMAND_CURVE_PATH << COMMAND_CURVE_POINT_SMOOTH << COMMAND_CURVE_POINT_X << COMMAND_CURVE_POINT_Y << COMMAND_CURVE_POINT_Z << COMMAND_CURVE_POINT_TRANSLATE2;
    QStringList forbiddenActions = QStringList() << COMMAND_POS_TRANSLATE;
    if(asCurvePoints.contains(QString(_property)))              propertyChanged(COMMAND_CURVE_POINT);
    else if(!forbiddenActions.contains(QString(_property)))     propertyChanged(_property);
    setProperty(_property, value);
}






QIcon NxObject::widgetIconActiveOff;
QIcon NxObject::widgetIconActiveOn;
QIcon NxObject::widgetIconSoloOff;
QIcon NxObject::widgetIconSoloOn;

void NxObject::setMute(quint16 _val) {
    objectMute = _val;
    if(objectMute)  setIcon(1, widgetIconActiveOff);
    else            setIcon(1, widgetIconActiveOn);
}
void NxObject::setSolo(quint16 _val) {
    objectSolo = _val;
    if(objectSolo)  setIcon(2, widgetIconSoloOn);
    else            setIcon(2, widgetIconSoloOff);
}

void NxObject::widgetClick(int col) {
    if(col == 1)        Application::current->execute(QString("%1 %2 %3").arg(COMMAND_MUTE).arg(id).arg(1-objectMute), ExecuteSourceGui);
    else if(col == 2)   Application::current->execute(QString("%1 %2 %3").arg(COMMAND_SOLO).arg(id).arg(1-objectSolo), ExecuteSourceGui);
}
