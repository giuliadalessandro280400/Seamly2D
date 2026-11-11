/******************************************************************************
 **  @file   base_library.h
 **
 **  @brief  Library of ready-made bases (draft blocks) that can be inserted into
 **          the open pattern with one click.
 **
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#ifndef BASE_LIBRARY_H
#define BASE_LIBRARY_H

#include <functional>

#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class VAbstractPattern;
class QDomDocument;

namespace BaseLibrary
{

struct Entry
{
    QString title;       // what the menu shows
    QString folder;      // submenu ("" = top level)
    QString path;        // file or resource path
    QString description; // tooltip
    bool    builtIn{true};
};

struct InsertResult
{
    bool        ok{false};
    QString     error;
    QStringList blocks;        // names of the inserted draft blocks
    QStringList renamedPoints; // "A -> A1"
    QStringList missing;       // measurements the formulas use but the pattern doesn't have
    QString     baseUnit;      // unit the base was written in
};

/// Names of the draft blocks of @p doc, read from the document itself.
QStringList blockNames(const QDomDocument &doc);

/// Folder where the user's own bases live (created on demand).
QString userFolder();

/// Built-in bases first, then the user's ones.
QVector<Entry> entries();

/// Reads a base file (converted to the current format) into @p out.
bool readBase(const QString &path, QDomDocument &out, QString *error = nullptr);

/**
 * Appends the draft blocks of the base at @p path to @p doc:
 * - every id is moved above the highest id of the pattern;
 * - a block with an existing name gets a number;
 * - a point whose name is taken gets a number, and the formulas follow;
 * - the base's variables the pattern lacks are copied;
 * - the blocks are moved so the first base point lands on @p start (pattern units).
 * @p knownNames are the measurement / variable names the pattern already has; any other name used by a
 * formula is reported in InsertResult::missing.
 * @p isTaken tells whether a point name is already used in the pattern.
 */
InsertResult insertBase(VAbstractPattern *doc, const QString &path, const QPointF &start,
                        const QSet<QString> &knownNames,
                        const std::function<bool(const QString &)> &isTaken);

/// Writes the whole pattern @p doc as a base named @p title into the user's folder. Returns the file path.
QString saveAsBase(const VAbstractPattern *doc, const QString &title, QString *error = nullptr);

} // namespace BaseLibrary

#endif // BASE_LIBRARY_H
