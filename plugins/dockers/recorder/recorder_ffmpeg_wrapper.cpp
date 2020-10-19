/*
 *  Copyright (c) 2020 Dmitrii Utkin <loentar@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "recorder_ffmpeg_wrapper.h"

#include <QDir>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

namespace
{

QRegularExpression frameRegexp("^frame=[ ]*([0-9]+) .*$");
QRegularExpression lineDelimiter("[\n\r]");
QRegularExpression junkRegex("\\[[a-zA-Z0-9]+ @ 0x[a-fA-F0-9]*\\][ ]*");
QStringList errorWords = { "Unable", "Invalid", "Error", "failed", "NULL", "No such", "divisible" };

// TODO: use QProcess::splitCommand after moving to Qt 5.15
/*!
    \since 5.15

    Splits the string \a command into a list of tokens, and returns
    the list.

    Tokens with spaces can be surrounded by double quotes; three
    consecutive double quotes represent the quote character itself.
*/
QStringList splitCommand(QStringView command)
{
    QStringList args;
    QString tmp;
    int quoteCount = 0;
    bool inQuote = false;

    // handle quoting. tokens can be surrounded by double quotes
    // "hello world". three consecutive double quotes represent
    // the quote character itself.
    for (int i = 0; i < command.size(); ++i) {
        if (command.at(i) == QLatin1Char('"')) {
            ++quoteCount;
            if (quoteCount == 3) {
                // third consecutive quote
                quoteCount = 0;
                tmp += command.at(i);
            }
            continue;
        }
        if (quoteCount) {
            if (quoteCount == 1)
                inQuote = !inQuote;
            quoteCount = 0;
        }
        if (!inQuote && command.at(i).isSpace()) {
            if (!tmp.isEmpty()) {
                args += tmp;
                tmp.clear();
            }
        } else {
            tmp += command.at(i);
        }
    }
    if (!tmp.isEmpty())
        args += tmp;

    return args;
}

}

RecorderFFMpegWrapper::RecorderFFMpegWrapper(QObject *parent)
    : QObject(parent)
{
}

void RecorderFFMpegWrapper::start(const RecorderFFMpegWrapperSettings &settings)
{
    Q_ASSERT(process == nullptr); // shall never happen

    readBuffer.clear();
    errorMessage.clear();

    process = new QProcess(this);
    process->setReadChannel(QProcess::ProcessChannel::StandardError);
    connect(process, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(process, SIGNAL(started()), SLOT(onStarted()));
    connect(process, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(onFinished(int)));

    const QString &arguments = "-y " % settings.arguments % " " % settings.outputFilePath;

    qDebug() << "starting ffmpeg: " << qUtf8Printable(settings.ffmpeg) << qUtf8Printable(arguments);

    const QStringList &args = splitCommand(arguments);

    process->start(settings.ffmpeg, args);
}

void RecorderFFMpegWrapper::kill()
{
    if (process == nullptr)
        return;

    process->disconnect(this);
    process->kill();
    process->deleteLater();
    process = nullptr;
}

void RecorderFFMpegWrapper::onReadyRead()
{
    readBuffer += process->readAll();

    int frameNo = -1;
    int startPos = 0;
    int endPos = 0;
    QString str;

    // ffmpeg splits normal lines by '\n' and progress data lines by '\r'
    while ((endPos = readBuffer.indexOf(lineDelimiter, startPos)) != -1) {
        const QString &line = readBuffer.mid(startPos, endPos - startPos).trimmed();

        for (const QString &word : errorWords) {
            if (line.contains(word)) {
                errorMessage += line % "\n";
                break;
            }
        }

        qDebug() << "ffmpeg:" << line;
        // frame=  314 fps=156 q=29.0 size=       0kB time=00:00:08.70 bitrate=   0.0kbits/s dup=52 drop=0 speed=4.32x
        const QRegularExpressionMatch &match = frameRegexp.match(line);
        if (match.hasMatch()) {
            frameNo = match.captured(1).toInt();
        }
        startPos = endPos + 1;
    }

    readBuffer.remove(0, startPos);

    if (frameNo != -1)
        emit progressUpdated(frameNo);

}

void RecorderFFMpegWrapper::onStarted()
{
    emit started();
}

void RecorderFFMpegWrapper::onFinished(int exitCode)
{
    qDebug() << "FFMpeg finished with code" << exitCode;
    if (exitCode != 0) {
        errorMessage.remove(junkRegex);
        emit finishedWithError(errorMessage);
    } else {
        emit finished();
    }

    process->deleteLater();
    process = nullptr;
}
