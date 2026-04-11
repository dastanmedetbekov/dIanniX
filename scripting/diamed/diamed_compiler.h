#ifndef DIAMED_COMPILER_H
#define DIAMED_COMPILER_H

#include <QString>
#include <QVector>

namespace Diamed {

enum class CompileMode {
    Studio,
    LiveStage
};

struct CompileOptions {
    CompileMode mode = CompileMode::Studio;
    int maxLines = 1200;
};

enum class Severity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    QString code;
    Severity severity = Severity::Error;
    int line = 1;
    int column = 1;
    QString message;
    QString hint;
};

struct CompileResult {
    bool ok = false;
    QString ir;
    QVector<Diagnostic> diagnostics;
    QVector<QString> executableCommands;
};

CompileResult compile(const QString &source, const CompileOptions &options = CompileOptions());
CompileResult compileForLiveStage(const QString &source);

}

#endif // DIAMED_COMPILER_H
