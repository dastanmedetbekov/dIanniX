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

#include "uieditor.h"
#include "ui_uieditor.h"

#include "iannix_cmd.h"

#include <QSettings>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QApplication>

namespace {
bool useLightEditorTheme() {
    if((Application::current) && (Application::current->getMainWindow())) {
        const QString mainStyle = Application::current->getMainWindow()->styleSheet();
        if(!mainStyle.trimmed().isEmpty())
            return false;
    }
    return Application::colorTheme;
}
}

UiEditor::UiEditor(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::UiEditor) {
    toolbarButton = 0;
    firstLaunch = true;
    embeddedMode = false;
    languageMode = LanguageMode::Legacy;
    languageModeCombo = 0;
    actionLiveReload = 0;
    completion = 0;
    completionModel = 0;

    ui->setupUi(this);
    applyTheme();

    if(QGuiApplication::primaryScreen()) {
        QRect screen = QGuiApplication::primaryScreen()->geometry();
        move(screen.bottomRight().x() - rect().width(), 20);
    }

    connect(ui->actionSave,        SIGNAL(triggered()), SLOT(save()));
    connect(ui->actionClose,       SIGNAL(triggered()), SLOT(close()));
    connect(ui->actionRefreshCode, SIGNAL(triggered()), SLOT(refresh()));

    setupLanguageControls();

    ui->jsEditor->setTextWrapEnabled(false);
    ui->jsEditor->setLineNumbersVisible(true);
    ui->jsEditor->setCodeFoldingEnabled(true);
    ui->jsEditor->setBracketsMatchingEnabled(true);
#ifdef QT6
    ui->jsEditor->setTabStopDistance(QFontMetricsF(ui->jsEditor->font()).horizontalAdvance(' ') * 4);
#else
    ui->jsEditor->setTabStopDistance(20);
#endif

    /*
    ui->jsEditor->setWindowTitle(QFileInfo(fileName).fileName());
    ui->jsEditor->setFrameShape(JSEdit::NoFrame);
    ui->jsEditor->setWordWrapMode(QTextOption::NoWrap);
#ifdef QT6
    ui->jsEditor->setTabStopDistance(QFontMetricsF(ui->jsEditor->font()).horizontalAdvance(' ') * 4);
#else
    ui->jsEditor->setTabStopDistance(4);
#endif
    ui->jsEditor->resize(QGuiApplication::primaryScreen()->availableGeometry().size() / 2);
    */
    /*
    ui->jsEditor->setColor(JSEdit::Background,    QColor("#0C152B"));
    ui->jsEditor->setColor(JSEdit::Normal,        QColor("#FFFFFF"));
    ui->jsEditor->setColor(JSEdit::Comment,       QColor("#666666"));
    ui->jsEditor->setColor(JSEdit::Number,        QColor("#DBF76C"));
    ui->jsEditor->setColor(JSEdit::String,        QColor("#5ED363"));
    ui->jsEditor->setColor(JSEdit::Operator,      QColor("#FF7729"));
    ui->jsEditor->setColor(JSEdit::Identifier,    QColor("#FFFFFF"));
    ui->jsEditor->setColor(JSEdit::Keyword,       QColor("#FDE15D"));
    ui->jsEditor->setColor(JSEdit::BuiltIn,       QColor("#9CB6D4"));
    ui->jsEditor->setColor(JSEdit::Cursor,        QColor("#1E346B"));
    ui->jsEditor->setColor(JSEdit::Marker,        QColor("#DBF76C"));
    ui->jsEditor->setColor(JSEdit::BracketMatch,  QColor("#1AB0A6"));
    ui->jsEditor->setColor(JSEdit::BracketError,  QColor("#A82224"));
    ui->jsEditor->setColor(JSEdit::FoldIndicator, QColor("#555555"));
    */

    ui->splitter_2->setStretchFactor(0, 5);
    ui->splitter_2->setStretchFactor(1, 1);

    ui->statusBar->setVisible(false);

    setupAutoCompletion();

    const int savedLanguageMode = QSettings().value("ui/editorLanguageMode", 0).toInt();
    if(savedLanguageMode == 1)
        setLanguageMode(LanguageMode::Diamed);
    else
        setLanguageMode(LanguageMode::Legacy);
}

UiEditor::~UiEditor() {
    delete ui;
}

void UiEditor::setContent(const QString &content, bool raiseWindow) {
    //contents.replace("\t", "  ");
    quint16 cursorPos = ui->jsEditor->textCursor().position();
    quint16 scrollPos = ui->jsEditor->verticalScrollBar()->sliderPosition();
    ui->jsEditor->setPlainText(content);
    if((!isVisible()) && (raiseWindow)) {
        if(!firstLaunch) {
            show();
            Application::current->getMainWindow()->raise();
        }
        firstLaunch = false;
    }
    QTextCursor cursor = ui->jsEditor->textCursor();
    cursor.setPosition(cursorPos);
    ui->jsEditor->setTextCursor(cursor);
    ui->jsEditor->verticalScrollBar()->setSliderPosition(scrollPos);
}
const QString UiEditor::getContent() {
    return ui->jsEditor->toPlainText();
}

void UiEditor::cursorChanged() {
    if(isDiamedMode())
        ui->help->scriptHelp(ui->jsEditor, QStringList() << "commands" << "values");
    else
        ui->help->scriptHelp(ui->jsEditor, QStringList() << "commands" << "javascript" << "values");
}

void UiEditor::scriptError(const QStringList &errors, qint16 line) {
    if(line < 0)
        ui->statusBar->setVisible(false);
    else {
        QString errorsMessage = "";
        foreach(const QString & error, errors)
            errorsMessage += error + " - ";

        QTextCursor cursorLine = QTextCursor(ui->jsEditor->document()->findBlockByLineNumber(line-1));
        ui->jsEditor->setTextCursor(cursorLine);
        ui->statusBar->setVisible(true);
        ui->statusBar->showMessage(errorsMessage);
        raise();
    }
}


void UiEditor::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::ActivationChange:
        refresh();
        break;
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void UiEditor::showEvent(QShowEvent *e) {
    applyTheme();
    if(toolbarButton)
        toolbarButton->setChecked(true);
    QMainWindow::showEvent(e);
}
void UiEditor::closeEvent(QCloseEvent *e) {
    if(embeddedMode) {
        emit(embeddedCloseRequested());
        e->ignore();
        return;
    }
    if(toolbarButton)
        toolbarButton->setChecked(false);
    QMainWindow::closeEvent(e);
}

void UiEditor::applyTheme() {
    if((Application::current) && (Application::current->getMainWindow()))
        setStyleSheet(Application::current->getMainWindow()->styleSheet());

    if(useLightEditorTheme()) {
        if((completion) && (completion->popup())) {
            completion->popup()->setStyleSheet(
                "QListView {"
                " background: #F4F6FA;"
                " color: #1F2530;"
                " border: 1px solid #B9C2D0;"
                " selection-background-color: #DDE8FF;"
                " selection-color: #182033;"
                "}");
        }
        ui->jsEditor->setColor(JSEdit::Background,    QColor("#F2F3F5"));
        ui->jsEditor->setColor(JSEdit::Normal,        QColor("#1F2530"));
        ui->jsEditor->setColor(JSEdit::Comment,       QColor("#7A7F8A"));
        ui->jsEditor->setColor(JSEdit::Number,        QColor("#3A7D44"));
        ui->jsEditor->setColor(JSEdit::String,        QColor("#9A4D2F"));
        ui->jsEditor->setColor(JSEdit::Operator,      QColor("#6E5A2E"));
        ui->jsEditor->setColor(JSEdit::Identifier,    QColor("#273A5D"));
        ui->jsEditor->setColor(JSEdit::Keyword,       QColor("#334FA0"));
        ui->jsEditor->setColor(JSEdit::BuiltIn,       QColor("#54688A"));
        ui->jsEditor->setColor(JSEdit::Sidebar,       QColor("#E1E4E8"));
        ui->jsEditor->setColor(JSEdit::LineNumber,    QColor("#5D6674"));
        ui->jsEditor->setColor(JSEdit::Cursor,        QColor("#DCE8FF"));
        ui->jsEditor->setColor(JSEdit::Marker,        QColor("#E2E8B5"));
        ui->jsEditor->setColor(JSEdit::BracketMatch,  QColor("#B7E4C7"));
        ui->jsEditor->setColor(JSEdit::BracketError,  QColor("#F2B8B5"));
        ui->jsEditor->setColor(JSEdit::FoldIndicator, QColor("#8A93A1"));
    }
    else {
        if((completion) && (completion->popup())) {
            completion->popup()->setStyleSheet(
                "QListView {"
                " background: #1B1F28;"
                " color: #DCE3F0;"
                " border: 1px solid #3E4759;"
                " selection-background-color: #3A4966;"
                " selection-color: #FFFFFF;"
                "}");
        }
        ui->jsEditor->setColor(JSEdit::Background,    QColor("#1C1F28"));
        ui->jsEditor->setColor(JSEdit::Normal,        QColor("#D8DDE8"));
        ui->jsEditor->setColor(JSEdit::Comment,       QColor("#8891A3"));
        ui->jsEditor->setColor(JSEdit::Number,        QColor("#9AD17D"));
        ui->jsEditor->setColor(JSEdit::String,        QColor("#E6B074"));
        ui->jsEditor->setColor(JSEdit::Operator,      QColor("#C5BEDA"));
        ui->jsEditor->setColor(JSEdit::Identifier,    QColor("#AFC9FF"));
        ui->jsEditor->setColor(JSEdit::Keyword,       QColor("#8FB4FF"));
        ui->jsEditor->setColor(JSEdit::BuiltIn,       QColor("#86A1C8"));
        ui->jsEditor->setColor(JSEdit::Sidebar,       QColor("#252A36"));
        ui->jsEditor->setColor(JSEdit::LineNumber,    QColor("#7F8AA1"));
        ui->jsEditor->setColor(JSEdit::Cursor,        QColor("#2F3A53"));
        ui->jsEditor->setColor(JSEdit::Marker,        QColor("#6B7346"));
        ui->jsEditor->setColor(JSEdit::BracketMatch,  QColor("#3C6D60"));
        ui->jsEditor->setColor(JSEdit::BracketError,  QColor("#8B3C4A"));
        ui->jsEditor->setColor(JSEdit::FoldIndicator, QColor("#5F6980"));
    }
}

void UiEditor::setupLanguageControls() {
    actionLiveReload = new QAction(tr("Live Reload"), this);
    actionLiveReload->setToolTip(tr("Save and reload current script without leaving editor"));
    actionLiveReload->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    connect(actionLiveReload, SIGNAL(triggered()), SLOT(liveReload()));
    ui->toolBar->insertAction(ui->actionRefreshCode, actionLiveReload);

    languageModeCombo = new QComboBox(this);
    languageModeCombo->addItem(tr("Legacy"));
    languageModeCombo->addItem(tr("Diamed"));
    languageModeCombo->setToolTip(tr("Script language mode for live coding"));
    connect(languageModeCombo, SIGNAL(currentIndexChanged(int)), SLOT(languageModeChanged(int)));
    ui->toolBar->addSeparator();
    ui->toolBar->addWidget(languageModeCombo);
}

void UiEditor::setupAutoCompletion() {
    completionModel = new QStringListModel(this);
    completion = new QCompleter(completionModel, this);
    completion->setCaseSensitivity(Qt::CaseInsensitive);
    completion->setCompletionMode(QCompleter::PopupCompletion);
    completion->setFilterMode(Qt::MatchContains);
    completion->setMaxVisibleItems(10);
    completion->popup()->setAlternatingRowColors(true);
    completion->setWidget(ui->jsEditor);
    connect(completion, SIGNAL(activated(QString)), SLOT(insertCompletion(QString)));

    ui->jsEditor->installEventFilter(this);
    rebuildCompletionEntries();
}

void UiEditor::rebuildCompletionEntries() {
    QStringList entries;
    commandCompletionEntries.clear();
    richCompletionEntries.clear();
    completionInsertions.clear();

    const QStringList commands = QStringList()
        << COMMAND_ADD
        << COMMAND_REMOVE
        << COMMAND_CLEAR
        << COMMAND_ID
        << COMMAND_GROUP
        << COMMAND_ACTIVE
        << COMMAND_CURSOR_CURVE
        << COMMAND_CURSOR_SPEED
        << COMMAND_CURSOR_SPEEDF
        << COMMAND_CURSOR_START
        << COMMAND_CURSOR_WIDTH
        << COMMAND_CURSOR_DEPTH
        << COMMAND_CURSOR_BOUNDS_SOURCE
        << COMMAND_CURSOR_BOUNDS_TARGET
        << COMMAND_CURSOR_OFFSET
        << COMMAND_CURVE_EQUATION
        << COMMAND_CURVE_EQUATION_PARAM
        << COMMAND_CURVE_EQUATION_POINTS
        << COMMAND_CURVE_ELL
        << COMMAND_CURVE_POINT
        << COMMAND_CURVE_POINT_SMOOTH
        << COMMAND_CURVE_POINT_RMV
        << COMMAND_SIZE
        << COMMAND_POS
        << COMMAND_POS_TRANSLATE
        << COMMAND_MESSAGE
        << COMMAND_MESSAGE_INTERVAL
        << COMMAND_LABEL
        << COMMAND_ZOOM
        << COMMAND_GOTO
        << COMMAND_PLAY
        << COMMAND_STOP
        << COMMAND_FF
        << COMMAND_SPEED
        << COMMAND_CENTER
        << COMMAND_ROTATE
        << COMMAND_TRIG
        << COMMAND_COLOR_GLOBAL
        << COMMAND_COLOR_GLOBAL_HUE
        << COMMAND_GLOBAL_COLOR
        << COMMAND_GLOBAL_COLOR_HUE
        << COMMAND_SOLO
        << COMMAND_MUTE
        << COMMAND_LOAD
        << COMMAND_OPEN
        << COMMAND_CLOSE
        << COMMAND_LOG
        << COMMAND_TITLE;

    foreach(const QString &command, commands) {
        commandCompletionEntries << command;
        entries << command;
    }

    if(isDiamedMode())
        entries << "legacy(\"\");";
    else
        entries << "run(\"\");";

    entries.removeDuplicates();
    entries.sort(Qt::CaseInsensitive);
    commandCompletionEntries.removeDuplicates();
    commandCompletionEntries.sort(Qt::CaseInsensitive);
    richCompletionEntries = entries;
    completionModel->setStringList(richCompletionEntries);
    completion->setCompletionPrefix(QString());
}

QString UiEditor::currentWordUnderCursor() const {
    QTextCursor cursor = ui->jsEditor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

void UiEditor::showCompletion(const QString &prefix, bool forceShowAll, bool commandOnly) {
    if((!completion) || (!completionModel))
        return;

    completionModel->setStringList(commandOnly ? commandCompletionEntries : richCompletionEntries);

    const QString appliedPrefix = forceShowAll ? QString() : prefix;
    completion->setCompletionPrefix(appliedPrefix);

    if((!forceShowAll) && (appliedPrefix.length() < 2)) {
        completion->popup()->hide();
        return;
    }

    QRect cr = ui->jsEditor->cursorRect();
    cr.setWidth(completion->popup()->sizeHintForColumn(0) + completion->popup()->verticalScrollBar()->sizeHint().width());
    completion->complete(cr);
}

void UiEditor::insertCompletion(const QString &selectedItem) {
    if((selectedItem == "run(\"\");") || (selectedItem == "legacy(\"\");")) {
        QTextCursor c = ui->jsEditor->textCursor();
        c.insertText(selectedItem);
        c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 3);
        ui->jsEditor->setTextCursor(c);
        return;
    }

    if(isInsideCommandString()) {
        QTextCursor c = ui->jsEditor->textCursor();
        c.select(QTextCursor::WordUnderCursor);
        c.removeSelectedText();
        c.insertText(selectedItem);
        ui->jsEditor->setTextCursor(c);
        return;
    }

    const QString wrapped = isDiamedMode()
        ? QString("legacy(\"%1\");").arg(selectedItem)
        : QString("run(\"%1\");").arg(selectedItem);

    QTextCursor c = ui->jsEditor->textCursor();
    c.select(QTextCursor::WordUnderCursor);
    c.removeSelectedText();
    c.insertText(wrapped);
    c.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 3);
    ui->jsEditor->setTextCursor(c);
}

bool UiEditor::isInsideCommandString() const {
    const QTextCursor cursor = ui->jsEditor->textCursor();
    const QString line = cursor.block().text();
    const int column = cursor.positionInBlock();
    const QString before = line.left(column);

    const int runIdx = before.lastIndexOf("run(\"");
    const int legacyIdx = before.lastIndexOf("legacy(\"");
    const int openIdx = qMax(runIdx, legacyIdx);
    if(openIdx < 0)
        return false;

    const int quoteAfterOpen = before.indexOf('"', openIdx);
    if(quoteAfterOpen < 0)
        return false;

    int quoteCount = 0;
    for(int i = quoteAfterOpen; i < before.length(); i++) {
        if((before.at(i) == '"') && ((i == 0) || (before.at(i - 1) != '\\')))
            quoteCount++;
    }
    return (quoteCount % 2) == 1;
}

bool UiEditor::eventFilter(QObject *obj, QEvent *event) {
    if((obj != ui->jsEditor) || (!completion))
        return QMainWindow::eventFilter(obj, event);

    if(event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if(completion->popup()->isVisible()) {
            switch(keyEvent->key()) {
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
            case Qt::Key_Home:
            case Qt::Key_End:
                QApplication::sendEvent(completion->popup(), event);
                return true;
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Tab:
                insertCompletion(completion->currentCompletion());
                completion->popup()->hide();
                return true;
            case Qt::Key_Escape:
                completion->popup()->hide();
                return true;
            default:
                break;
            }
        }

        const bool isCompletionShortcut =
            (keyEvent->key() == Qt::Key_Period) &&
            (keyEvent->modifiers() == Qt::ControlModifier);
        const bool isSecondaryShortcut =
            (keyEvent->key() == Qt::Key_Slash) &&
            (keyEvent->modifiers() == Qt::AltModifier);
        if(isCompletionShortcut || isSecondaryShortcut) {
            showCompletion(QString(), true, isInsideCommandString());
            return true;
        }
    }
    else if(event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if(completion->popup()->isVisible()) {
            if((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down) ||
               (keyEvent->key() == Qt::Key_PageUp) || (keyEvent->key() == Qt::Key_PageDown))
                return true;
        }
        if(keyEvent->modifiers() == Qt::NoModifier) {
            const QString prefix = currentWordUnderCursor();
            showCompletion(prefix, false, isInsideCommandString());
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void UiEditor::setLanguageMode(LanguageMode mode) {
    languageMode = mode;
    if(languageModeCombo) {
        const int index = (mode == LanguageMode::Diamed) ? 1 : 0;
        if(languageModeCombo->currentIndex() != index)
            languageModeCombo->setCurrentIndex(index);
    }
    QSettings().setValue("ui/editorLanguageMode", (mode == LanguageMode::Diamed) ? 1 : 0);
    rebuildCompletionEntries();
    cursorChanged();
}

void UiEditor::languageModeChanged(int index) {
    setLanguageMode((index == 1) ? LanguageMode::Diamed : LanguageMode::Legacy);
}
