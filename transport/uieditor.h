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

#ifndef UIEDITOR_H
#define UIEDITOR_H

#include <QMainWindow>
#include <QFileInfo>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextCursor>
#include <QComboBox>
#include <QCompleter>
#include <QStringListModel>
#include <QHash>
#include "misc/help.h"
#include "misc/application.h"


namespace Ui {
    class UiEditor;
}

class UiEditor : public QMainWindow {
    Q_OBJECT

public:
    enum class LanguageMode {
        Legacy = 0,
        Diamed = 1
    };

public:
    explicit UiEditor(QWidget *parent = 0);
    ~UiEditor();
    void setEmbeddedMode(bool enabled) { embeddedMode = enabled; }
    void setLanguageMode(LanguageMode mode);
    LanguageMode getLanguageMode() const { return languageMode; }
    bool isDiamedMode() const { return languageMode == LanguageMode::Diamed; }

public:
    void setContent(const QString &content, bool raiseWindow);
    const QString getContent();

signals:
    void askSave();
    void askRefresh();
    void askLiveReload();
    void embeddedCloseRequested();

public slots:
    void save()     { emit(askSave()); }
    void refresh()  { emit(askRefresh()); }
    void liveReload() { emit(askLiveReload()); }
    void cursorChanged();
    void scriptError(const QStringList &errors, qint16 line);
    void languageModeChanged(int index);

public:
    QAction *toolbarButton;
    bool firstLaunch;
    bool embeddedMode;
    LanguageMode languageMode;
protected:
    bool eventFilter(QObject *obj, QEvent *event);
    void changeEvent(QEvent *e);
    void showEvent(QShowEvent *);
    void closeEvent(QCloseEvent *);
private:
    void applyTheme();
    void setupLanguageControls();
    void setupAutoCompletion();
    void rebuildCompletionEntries();
    void showCompletion(const QString &prefix, bool forceShowAll, bool commandOnly);
    QString currentWordUnderCursor() const;
    bool isInsideCommandString() const;
    Ui::UiEditor *ui;
    QComboBox *languageModeCombo;
    QAction *actionLiveReload;
    QCompleter *completion;
    QStringListModel *completionModel;
    QHash<QString, QString> completionInsertions;
    QStringList commandCompletionEntries;
    QStringList richCompletionEntries;

private slots:
    void insertCompletion(const QString &selectedItem);
};

#endif // UIEDITOR_H
