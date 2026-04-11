#include "diamed_compiler.h"

#include <QChar>
#include <QRegularExpression>
#include <QStringList>

namespace Diamed {
namespace {

static Diagnostic makeDiagnostic(
    const QString &code,
    Severity severity,
    int line,
    int column,
    const QString &message,
    const QString &hint = QString()) {
    Diagnostic d;
    d.code = code;
    d.severity = severity;
    d.line = line;
    d.column = column;
    d.message = message;
    d.hint = hint;
    return d;
}

static bool looksLikeControlHeader(const QString &trimmedLine) {
    return trimmedLine.startsWith("if ") ||
           trimmedLine.startsWith("if(") ||
           trimmedLine.startsWith("for ") ||
           trimmedLine.startsWith("for(") ||
           trimmedLine.startsWith("while ") ||
           trimmedLine.startsWith("while(") ||
           trimmedLine.startsWith("fn ") ||
           trimmedLine.startsWith("on ");
}

static bool requiresSemicolon(const QString &trimmedLine) {
    if(trimmedLine.isEmpty())
        return false;
    if(trimmedLine.startsWith("//"))
        return false;
    if(trimmedLine.endsWith("{"))
        return false;
    if(trimmedLine.endsWith("}"))
        return false;
    if(looksLikeControlHeader(trimmedLine))
        return false;
    return true;
}

static int firstIndexAny(const QString &line, const QStringList &patterns) {
    int best = -1;
    foreach(const QString &pattern, patterns) {
        const int idx = line.indexOf(pattern, 0, Qt::CaseInsensitive);
        if((idx >= 0) && ((best < 0) || (idx < best)))
            best = idx;
    }
    return best;
}

static QString unescapeQuotedString(const QString &value) {
    QString unescaped = value;
    unescaped.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    unescaped.replace(QStringLiteral("\\\'"), QStringLiteral("\'"));
    unescaped.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    unescaped.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
    unescaped.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return unescaped;
}

static QString extractLegacyCommand(const QString &trimmedLine) {
    static const QRegularExpression legacyDouble(
        QStringLiteral("^legacy\\s*\\(\\s*\\\"((?:\\\\.|[^\\\"])*)\\\"\\s*\\)\\s*;?\\s*$"));
    static const QRegularExpression legacySingle(
        QStringLiteral("^legacy\\s*\\(\\s*'((?:\\\\.|[^'])*)'\\s*\\)\\s*;?\\s*$"));

    QRegularExpressionMatch match = legacyDouble.match(trimmedLine);
    if(match.hasMatch())
        return unescapeQuotedString(match.captured(1));

    match = legacySingle.match(trimmedLine);
    if(match.hasMatch())
        return unescapeQuotedString(match.captured(1));

    return QString();
}

}

CompileResult compile(const QString &source, const CompileOptions &options) {
    CompileResult result;
    result.ok = true;

    int braceBalance = 0;
    int parenBalance = 0;

    const QStringList lines = source.split('\n');
    if((options.maxLines > 0) && (lines.count() > options.maxLines)) {
        result.ok = false;
        result.diagnostics.append(makeDiagnostic(
            "E0401",
            Severity::Error,
            options.maxLines,
            1,
            QString("live patch too large (%1 lines, limit %2)").arg(lines.count()).arg(options.maxLines),
            "split patch into smaller performance scenes"
        ));
    }

    const QStringList forbiddenLiveTokens = QStringList()
        << "while(true" << "while (true" << "for(;;" << "for (;;"
        << "sleep(" << "wait(" << "system(" << "qprocess" << "thread(";
    const QStringList unstableLiveTokens = QStringList()
        << "rand(" << "random(";

    for(int i = 0; i < lines.count(); i++) {
        const QString line = lines.at(i);
        const QString trimmed = line.trimmed();

        const QString command = extractLegacyCommand(trimmed);
        if(!command.isEmpty())
            result.executableCommands.append(command);

        for(int j = 0; j < line.size(); j++) {
            const QChar c = line.at(j);
            if(c == '{') braceBalance++;
            else if(c == '}') braceBalance--;
            else if(c == '(') parenBalance++;
            else if(c == ')') parenBalance--;

            if(braceBalance < 0) {
                result.ok = false;
                result.diagnostics.append(makeDiagnostic(
                    "E0103",
                    Severity::Error,
                    i + 1,
                    j + 1,
                    "unexpected '}'",
                    "remove extra closing brace or add matching '{' before"
                ));
                braceBalance = 0;
            }

            if(parenBalance < 0) {
                result.ok = false;
                result.diagnostics.append(makeDiagnostic(
                    "E0104",
                    Severity::Error,
                    i + 1,
                    j + 1,
                    "unexpected ')'",
                    "remove extra ')' or add matching '(' before"
                ));
                parenBalance = 0;
            }
        }

        if(requiresSemicolon(trimmed) && !trimmed.endsWith(';')) {
            result.ok = false;
            result.diagnostics.append(makeDiagnostic(
                "E0102",
                Severity::Error,
                i + 1,
                line.size(),
                "expected ';' after expression",
                "append ';' at end of statement"
            ));
        }

        if(trimmed.startsWith("legacy(") && !trimmed.contains(')')) {
            result.ok = false;
            result.diagnostics.append(makeDiagnostic(
                "E0201",
                Severity::Error,
                i + 1,
                1,
                "unterminated legacy(...) call",
                "close call with ')' and ';'"
            ));
        }

        if(options.mode == CompileMode::LiveStage) {
            const int forbiddenIndex = firstIndexAny(trimmed, forbiddenLiveTokens);
            if(forbiddenIndex >= 0) {
                result.ok = false;
                result.diagnostics.append(makeDiagnostic(
                    "E0402",
                    Severity::Error,
                    i + 1,
                    forbiddenIndex + 1,
                    "realtime-unsafe construct in live stage mode",
                    "remove blocking/infinite execution paths from performance patch"
                ));
            }

            const int unstableIndex = firstIndexAny(trimmed, unstableLiveTokens);
            if(unstableIndex >= 0) {
                result.diagnostics.append(makeDiagnostic(
                    "W0403",
                    Severity::Warning,
                    i + 1,
                    unstableIndex + 1,
                    "non-deterministic randomness detected",
                    "prefer deterministic generators with explicit seeds"
                ));
            }
        }
    }

    if(braceBalance != 0) {
        result.ok = false;
        result.diagnostics.append(makeDiagnostic(
            "E0105",
            Severity::Error,
            lines.count(),
            1,
            "unbalanced braces",
            "ensure each '{' has matching '}'"
        ));
    }

    if(parenBalance != 0) {
        result.ok = false;
        result.diagnostics.append(makeDiagnostic(
            "E0106",
            Severity::Error,
            lines.count(),
            1,
            "unbalanced parentheses",
            "ensure each '(' has matching ')'"
        ));
    }

    if(result.ok) {
        result.ir = QString("; DIAMED-IR v0\n") + source;
        result.diagnostics.append(makeDiagnostic(
            "I0001",
            Severity::Info,
            1,
            1,
            "compile succeeded"
        ));
    }

    return result;
}

CompileResult compileForLiveStage(const QString &source) {
    CompileOptions options;
    options.mode = CompileMode::LiveStage;
    options.maxLines = 500;
    return compile(source, options);
}

}
