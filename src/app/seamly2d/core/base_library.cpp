/******************************************************************************
 **  @file   base_library.cpp
 **
 **  @brief  Library of ready-made bases (draft blocks) that can be inserted into
 **          the open pattern with one click.
 **
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#include "base_library.h"

#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/xml/vpatternconverter.h"
#include "../ifc/ifcdef.h"
#include "../vmisc/def.h"

#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

namespace
{

const QString baseResourceDir = QStringLiteral(":/bases");

// Attributes that hold the id of another object of the pattern.
const QSet<QString> &idAttributes()
{
    static const QSet<QString> attrs{
        QStringLiteral("id"), QStringLiteral("basePoint"), QStringLiteral("firstPoint"),
        QStringLiteral("secondPoint"), QStringLiteral("thirdPoint"), QStringLiteral("center"),
        QStringLiteral("p1Line"), QStringLiteral("p2Line"), QStringLiteral("p1Line1"), QStringLiteral("p2Line1"),
        QStringLiteral("p1Line2"), QStringLiteral("p2Line2"), QStringLiteral("pShoulder"),
        QStringLiteral("point1"), QStringLiteral("point2"), QStringLiteral("point3"), QStringLiteral("point4"),
        QStringLiteral("pSpline"), QStringLiteral("axisP1"), QStringLiteral("axisP2"), QStringLiteral("curve"),
        QStringLiteral("curve1"), QStringLiteral("curve2"), QStringLiteral("firstArc"),
        QStringLiteral("secondArc"), QStringLiteral("c1Center"), QStringLiteral("c2Center"),
        QStringLiteral("cCenter"), QStringLiteral("tangent"), QStringLiteral("arc"), QStringLiteral("idObject"),
        QStringLiteral("baseLineP1"), QStringLiteral("baseLineP2"), QStringLiteral("dartP1"),
        QStringLiteral("dartP2"), QStringLiteral("dartP3"), QStringLiteral("spline"),
        QStringLiteral("splinePath"), QStringLiteral("sourcePoint"), QStringLiteral("targetPoint"),
        QStringLiteral("sourceDirection"), QStringLiteral("targetDirection"), QStringLiteral("hinge"),
        QStringLiteral("edgeStart"), QStringLiteral("edgeEnd"), QStringLiteral("anchor"),
        QStringLiteral("direction"), QStringLiteral("dxfAnchor"), QStringLiteral("dxfDirection"),
        QStringLiteral("object"), QStringLiteral("tool"), QStringLiteral("idTool"), QStringLiteral("start"),
        QStringLiteral("path"), QStringLiteral("end"), QStringLiteral("topLeftAnchor"),
        QStringLiteral("bottomRightAnchor"), QStringLiteral("centerAnchor"), QStringLiteral("topAnchor"),
        QStringLiteral("bottomAnchor")};
    return attrs;
}

// Elements whose text is the id of an object.
const QSet<QString> &idTextElements()
{
    static const QSet<QString> tags{QStringLiteral("record"), QStringLiteral("child")};
    return tags;
}

// Attributes that hold a point's name.
const QStringList &nameAttributes()
{
    static const QStringList attrs{QStringLiteral("name"), QStringLiteral("name1"), QStringLiteral("name2")};
    return attrs;
}

// Attributes that never hold a formula.
const QSet<QString> &notFormulas()
{
    static const QSet<QString> attrs{
        QStringLiteral("name"), QStringLiteral("name1"), QStringLiteral("name2"), QStringLiteral("type"),
        QStringLiteral("lineType"), QStringLiteral("lineWeight"), QStringLiteral("lineColor"),
        QStringLiteral("color"), QStringLiteral("penStyle"), QStringLiteral("mx"), QStringLiteral("my"),
        QStringLiteral("mx1"), QStringLiteral("my1"), QStringLiteral("mx2"), QStringLiteral("my2"),
        QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("showPointName"),
        QStringLiteral("showPointName1"), QStringLiteral("showPointName2"), QStringLiteral("suffix"),
        QStringLiteral("description"), QStringLiteral("axisType"), QStringLiteral("crossPoint"),
        QStringLiteral("vCrossPoint"), QStringLiteral("hCrossPoint"), QStringLiteral("objName"),
        QStringLiteral("alias"), QStringLiteral("notes"), QStringLiteral("letter"),
        QStringLiteral("annotation"), QStringLiteral("orientation"), QStringLiteral("rotationWay"),
        QStringLiteral("tilt"), QStringLiteral("foldPosition"), QStringLiteral("fill"),
        QStringLiteral("reference"), QStringLiteral("links"), QStringLiteral("xy"),
        QStringLiteral("spreadType"), QStringLiteral("fixedSide"), QStringLiteral("curveOffsets"),
        QStringLiteral("curvePins"), QStringLiteral("typeObject"), QStringLiteral("nodeType"),
        QStringLiteral("groupColor"), QStringLiteral("visible"), QStringLiteral("locked"),
        QStringLiteral("inUse"), QStringLiteral("closed"), QStringLiteral("cut"), QStringLiteral("lengthMode"),
        QStringLiteral("autoSmooth")};
    return attrs;
}

// Built-in variable prefixes whose parts after the prefix are point names (Line_A_B, Spl_A_B...).
const QStringList &pointVariablePrefixes()
{
    // Longest first so that Angle1SplPath wins over Angle1 etc.
    static QStringList prefixes = []()
    {
        QStringList list = builInVariables;
        list.removeAll(measurement_);
        list.removeAll(variable_);
        std::sort(list.begin(), list.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
        return list;
    }();
    return prefixes;
}

bool isPointVariable(const QString &token)
{
    for (const QString &prefix : pointVariablePrefixes())
    {
        if (token.startsWith(prefix))
        {
            return true;
        }
    }
    return false;
}

const QRegularExpression &identifierRe()
{
    // Names in formulas: letters, digits, '_', '@' (custom measurements) and '#' (variables).
    static const QRegularExpression re(QStringLiteral("[A-Za-z_@#\\x{00C0}-\\x{024F}][\\w@#.]*"),
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

bool looksLikeFormula(const QString &value)
{
    bool isNumber = false;
    value.toDouble(&isNumber);
    return !isNumber && !value.trimmed().isEmpty();
}

// Renames the point names inside the built-in variables of @p formula.
QString renameInFormula(const QString &formula, const QHash<QString, QString> &renames)
{
    if (renames.isEmpty())
    {
        return formula;
    }
    QString result;
    int last = 0;
    QRegularExpressionMatchIterator it = identifierRe().globalMatch(formula);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        result += formula.mid(last, m.capturedStart() - last);
        QString token = m.captured();
        if (isPointVariable(token))
        {
            QStringList parts = token.split(QLatin1Char('_'));
            for (int i = 1; i < parts.size(); ++i)
            {
                if (renames.contains(parts.at(i)))
                {
                    parts[i] = renames.value(parts.at(i));
                }
            }
            token = parts.join(QLatin1Char('_'));
        }
        result += token;
        last = m.capturedEnd();
    }
    result += formula.mid(last);
    return result;
}

// Names used by @p formula that are neither built-in variables nor functions.
QStringList namesInFormula(const QString &formula)
{
    QStringList names;
    QRegularExpressionMatchIterator it = identifierRe().globalMatch(formula);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const QString token = m.captured();
        // a function call: name followed by '('
        int next = m.capturedEnd();
        while (next < formula.size() && formula.at(next).isSpace())
        {
            ++next;
        }
        if (next < formula.size() && formula.at(next) == QLatin1Char('('))
        {
            continue;
        }
        if (isPointVariable(token) || token == currentLength || token == currentSeamAllowance)
        {
            continue;
        }
        if (token == QLatin1String("pi") || token == QLatin1String("e") || token == QLatin1String("_pi")
            || token == QLatin1String("_e"))
        {
            continue;
        }
        names.append(token);
    }
    return names;
}

template <typename F>
void forEachElement(const QDomElement &root, F f)
{
    QDomElement child = root.firstChildElement();
    while (!child.isNull())
    {
        f(child);
        forEachElement(child, f);
        child = child.nextSiblingElement();
    }
}

QString titleFromFile(const QString &fileName)
{
    QString title = QFileInfo(fileName).completeBaseName();
    title.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!title.isEmpty())
    {
        title[0] = title.at(0).toUpper();
    }
    return title;
}

QString descriptionOf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return QString();
    }
    QDomDocument dom;
    if (!dom.setContent(&file))
    {
        return QString();
    }
    return dom.documentElement().firstChildElement(QStringLiteral("description")).text().trimmed();
}

void collect(const QString &root, bool builtIn, QVector<BaseLibrary::Entry> &out)
{
    QDirIterator it(root, QStringList{QStringLiteral("*.sm2d"), QStringLiteral("*.val")}, QDir::Files,
                    QDirIterator::Subdirectories);
    QVector<BaseLibrary::Entry> found;
    while (it.hasNext())
    {
        const QString path = it.next();
        BaseLibrary::Entry entry;
        entry.path = path;
        entry.builtIn = builtIn;
        entry.title = titleFromFile(path);
        entry.folder = QDir(root).relativeFilePath(QFileInfo(path).path());
        if (entry.folder == QLatin1String("."))
        {
            entry.folder.clear();
        }
        entry.description = descriptionOf(path);
        found.append(entry);
    }
    std::sort(found.begin(), found.end(), [](const BaseLibrary::Entry &a, const BaseLibrary::Entry &b)
    {
        if (a.folder != b.folder)
        {
            return a.folder.localeAwareCompare(b.folder) < 0;
        }
        return a.title.localeAwareCompare(b.title) < 0;
    });
    out += found;
}

quint32 maxId(const QDomElement &root)
{
    quint32 result = 0;
    forEachElement(root, [&result](const QDomElement &e)
    {
        bool ok = false;
        const quint32 id = e.attribute(QStringLiteral("id")).toUInt(&ok);
        if (ok)
        {
            result = qMax(result, id);
        }
    });
    return result;
}

} // namespace

//---------------------------------------------------------------------------------------------------------------------
QStringList BaseLibrary::blockNames(const QDomDocument &doc)
{
    QStringList names;
    const QDomElement root = doc.documentElement();
    for (QDomElement b = root.firstChildElement(QStringLiteral("draftBlock")); !b.isNull();
         b = b.nextSiblingElement(QStringLiteral("draftBlock")))
    {
        names.append(b.attribute(QStringLiteral("name")));
    }
    return names;
}

//---------------------------------------------------------------------------------------------------------------------
QString BaseLibrary::userFolder()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty())
    {
        documents = QDir::homePath();
    }
    const QString folder = documents + QStringLiteral("/Seamly2D/") + QObject::tr("Bases");
    QDir().mkpath(folder);
    return folder;
}

//---------------------------------------------------------------------------------------------------------------------
QVector<BaseLibrary::Entry> BaseLibrary::entries()
{
    QVector<Entry> result;
    collect(baseResourceDir, true, result);
    collect(userFolder(), false, result);
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool BaseLibrary::readBase(const QString &path, QDomDocument &out, QString *error)
{
    try
    {
        // The converter needs a real file: resources are copied out first.
        QString source = path;
        QString temporary;
        if (path.startsWith(QLatin1Char(':')))
        {
            temporary = QDir::temp().filePath(QStringLiteral("seamly2d_base_%1.sm2d")
                                              .arg(QCoreApplication::applicationPid()));
            QFile::remove(temporary);
            if (!QFile::copy(path, temporary))
            {
                if (error)
                {
                    *error = QObject::tr("Can't read the base %1").arg(path);
                }
                return false;
            }
            QFile::setPermissions(temporary, QFile::ReadOwner | QFile::WriteOwner);
            source = temporary;
        }

        VPatternConverter converter(source);
        const QString converted = converter.Convert();
        QFile file(converted);
        const bool opened = file.open(QIODevice::ReadOnly);
        const bool parsed = opened && out.setContent(&file);
        file.close();
        if (!temporary.isEmpty())
        {
            QFile::remove(temporary);
        }
        if (!parsed)
        {
            if (error)
            {
                *error = QObject::tr("Can't read the base %1").arg(path);
            }
            return false;
        }
        return true;
    }
    catch (VException &e)
    {
        if (error)
        {
            *error = e.ErrorMessage();
        }
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------
BaseLibrary::InsertResult BaseLibrary::insertBase(VAbstractPattern *doc, const QString &path, const QPointF &start,
                                                  const QSet<QString> &knownNames,
                                                  const std::function<bool(const QString &)> &isTaken)
{
    InsertResult result;

    QDomDocument base;
    if (!readBase(path, base, &result.error))
    {
        return result;
    }

    const QDomElement baseRoot = base.documentElement();
    result.baseUnit = baseRoot.firstChildElement(QStringLiteral("unit")).text();

    QVector<QDomElement> blocks;
    for (QDomElement b = baseRoot.firstChildElement(QStringLiteral("draftBlock")); !b.isNull();
         b = b.nextSiblingElement(QStringLiteral("draftBlock")))
    {
        blocks.append(b);
    }
    if (blocks.isEmpty())
    {
        result.error = QObject::tr("The base %1 has no draft blocks").arg(path);
        return result;
    }

    // 1. New ids above the pattern's highest one.
    const quint32 offset = maxId(doc->documentElement());
    QSet<quint32> baseIds;
    for (const QDomElement &block : blocks)
    {
        forEachElement(block, [&baseIds](const QDomElement &e)
        {
            bool ok = false;
            const quint32 id = e.attribute(QStringLiteral("id")).toUInt(&ok);
            if (ok && id > 0)
            {
                baseIds.insert(id);
            }
        });
    }
    auto remap = [&baseIds, offset](const QString &value, bool *changed) -> QString
    {
        bool ok = false;
        const quint32 id = value.toUInt(&ok);
        if (ok && baseIds.contains(id))
        {
            *changed = true;
            return QString::number(id + offset);
        }
        *changed = false;
        return value;
    };

    // 2. Point names that the pattern already uses.
    QSet<QString> baseNames;
    for (const QDomElement &block : blocks)
    {
        forEachElement(block.firstChildElement(QStringLiteral("calculation")), [&baseNames](const QDomElement &e)
        {
            for (const QString &attr : nameAttributes())
            {
                const QString name = e.attribute(attr);
                if (!name.isEmpty())
                {
                    baseNames.insert(name);
                }
            }
        });
    }
    QHash<QString, QString> renames;
    QStringList conflicts;
    for (const QString &name : baseNames)
    {
        if (isTaken(name))
        {
            conflicts.append(name);
        }
    }
    if (!conflicts.isEmpty())
    {
        // One number for the whole base, so that A, B, F0 become A1, B1, F01.
        std::sort(conflicts.begin(), conflicts.end());
        for (int n = 1; n < 10000; ++n)
        {
            bool free = true;
            for (const QString &name : conflicts)
            {
                const QString candidate = name + QString::number(n);
                if (isTaken(candidate) || baseNames.contains(candidate))
                {
                    free = false;
                    break;
                }
            }
            if (free)
            {
                for (const QString &name : conflicts)
                {
                    renames.insert(name, name + QString::number(n));
                    result.renamedPoints.append(name + QStringLiteral(" \u2192 ") + name + QString::number(n));
                }
                break;
            }
        }
    }

    // 3. Where the blocks go: the first base point lands on start.
    QPointF shift;
    {
        bool found = false;
        for (const QDomElement &block : blocks)
        {
            forEachElement(block.firstChildElement(QStringLiteral("calculation")),
                           [&found, &shift, &start](const QDomElement &e)
            {
                if (!found && e.tagName() == QLatin1String("point")
                    && e.attribute(QStringLiteral("type")) == QLatin1String("single"))
                {
                    shift = start - QPointF(e.attribute(QStringLiteral("x")).toDouble(),
                                            e.attribute(QStringLiteral("y")).toDouble());
                    found = true;
                }
            });
        }
    }

    // 4. Rewrite every element of the blocks.
    QSet<QString> formulaNames;
    for (QDomElement &block : blocks)
    {
        forEachElement(block, [&](QDomElement e)
        {
            const QDomNamedNodeMap attrs = e.attributes();
            QVector<QPair<QString, QString>> changes;
            for (int i = 0; i < attrs.count(); ++i)
            {
                const QDomAttr attr = attrs.item(i).toAttr();
                const QString name = attr.name();
                const QString value = attr.value();

                if (idAttributes().contains(name))
                {
                    bool changed = false;
                    const QString v = remap(value, &changed);
                    if (changed)
                    {
                        changes.append(qMakePair(name, v));
                        continue;
                    }
                }

                if (nameAttributes().contains(name) && renames.contains(value))
                {
                    changes.append(qMakePair(name, renames.value(value)));
                    continue;
                }

                if (name == QLatin1String("links"))
                {
                    // shape:point:x,y;... (DXF import ties)
                    QStringList links = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                    for (QString &link : links)
                    {
                        QStringList parts = link.split(QLatin1Char(':'));
                        if (parts.size() >= 2)
                        {
                            bool changed = false;
                            parts[1] = remap(parts.at(1), &changed);
                            link = parts.join(QLatin1Char(':'));
                        }
                    }
                    changes.append(qMakePair(name, links.join(QLatin1Char(';'))));
                    continue;
                }

                if (!notFormulas().contains(name) && !idAttributes().contains(name) && looksLikeFormula(value))
                {
                    const QString renamed = renameInFormula(value, renames);
                    if (renamed != value)
                    {
                        changes.append(qMakePair(name, renamed));
                    }
                    for (const QString &n : namesInFormula(renamed))
                    {
                        formulaNames.insert(n);
                    }
                }
            }

            if (e.tagName() == QLatin1String("point") && e.attribute(QStringLiteral("type")) == QLatin1String("single"))
            {
                const double x = e.attribute(QStringLiteral("x")).toDouble() + shift.x();
                const double y = e.attribute(QStringLiteral("y")).toDouble() + shift.y();
                changes.append(qMakePair(QStringLiteral("x"), QString::number(x, 'f', 4)));
                changes.append(qMakePair(QStringLiteral("y"), QString::number(y, 'f', 4)));
            }

            for (const auto &c : changes)
            {
                e.setAttribute(c.first, c.second);
            }

            if (idTextElements().contains(e.tagName()) && e.firstChildElement().isNull())
            {
                bool changed = false;
                const QString v = remap(e.text().trimmed(), &changed);
                if (changed)
                {
                    while (e.hasChildNodes())
                    {
                        e.removeChild(e.firstChild());
                    }
                    e.appendChild(e.ownerDocument().createTextNode(v));
                }
            }
        });
    }

    // 5. Variables of the base the pattern doesn't have yet.
    QDomElement docRoot = doc->documentElement();
    QSet<QString> baseVariables;
    {
        const QDomElement baseVars = baseRoot.firstChildElement(QStringLiteral("variables"));
        QDomElement docVars = docRoot.firstChildElement(QStringLiteral("variables"));
        QSet<QString> existing;
        for (QDomElement v = docVars.firstChildElement(); !v.isNull(); v = v.nextSiblingElement())
        {
            existing.insert(v.attribute(QStringLiteral("name")));
        }
        for (QDomElement v = baseVars.firstChildElement(); !v.isNull(); v = v.nextSiblingElement())
        {
            const QString name = v.attribute(QStringLiteral("name"));
            baseVariables.insert(name);
            for (const QString &n : namesInFormula(v.attribute(QStringLiteral("formula"))))
            {
                formulaNames.insert(n);
            }
            if (existing.contains(name) || docVars.isNull())
            {
                continue;
            }
            docVars.appendChild(doc->importNode(v, true));
        }
    }

    // 6. Names the formulas need that nobody provides.
    for (const QString &n : formulaNames)
    {
        if (!knownNames.contains(n) && !baseVariables.contains(n) && !result.missing.contains(n))
        {
            result.missing.append(n);
        }
    }
    std::sort(result.missing.begin(), result.missing.end());

    // 7. Block names and the insertion itself.
    const QStringList existingBlocks = blockNames(*doc);
    QDomElement after;
    for (QDomElement b = docRoot.firstChildElement(QStringLiteral("draftBlock")); !b.isNull();
         b = b.nextSiblingElement(QStringLiteral("draftBlock")))
    {
        after = b;
    }
    QStringList usedBlocks = existingBlocks;
    for (const QDomElement &block : blocks)
    {
        QDomElement copy = doc->importNode(block, true).toElement();
        QString name = copy.attribute(QStringLiteral("name"));
        if (usedBlocks.contains(name))
        {
            int n = 2;
            QString candidate;
            do
            {
                candidate = QStringLiteral("%1 %2").arg(name).arg(n++);
            } while (usedBlocks.contains(candidate));
            name = candidate;
            copy.setAttribute(QStringLiteral("name"), name);
        }
        usedBlocks.append(name);
        result.blocks.append(name);

        if (after.isNull())
        {
            docRoot.appendChild(copy);
        }
        else
        {
            docRoot.insertAfter(copy, after);
        }
        after = copy;
    }

    result.ok = true;
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
QString BaseLibrary::saveAsBase(const VAbstractPattern *doc, const QString &title, QString *error)
{
    QString fileName = title.trimmed();
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    if (fileName.isEmpty())
    {
        fileName = QObject::tr("Base");
    }
    const QString path = userFolder() + QLatin1Char('/') + fileName + QStringLiteral(".sm2d");

    QDomDocument copy = doc->cloneNode(true).toDocument();
    QDomElement root = copy.documentElement();
    // A base doesn't carry a measurement file: it uses the one of the pattern it goes into.
    const QDomElement measurements = root.firstChildElement(QStringLiteral("measurements"));
    if (!measurements.isNull())
    {
        QDomElement empty = copy.createElement(QStringLiteral("measurements"));
        root.replaceChild(empty, measurements);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error)
        {
            *error = file.errorString();
        }
        return QString();
    }
    QTextStream out(&file);
    out << copy.toString(4);
    return path;
}
