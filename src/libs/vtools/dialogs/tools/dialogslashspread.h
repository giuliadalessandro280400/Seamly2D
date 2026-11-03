/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogslashspread.h
 **  @brief  Dialog of slash and spread
 **           
 **
 **  Seamly2D is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Seamly2D is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Seamly2D.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#ifndef DIALOGSLASHSPREAD_H
#define DIALOGSLASHSPREAD_H

#include <qcompilerdetection.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "../vmisc/def.h"
#include "../../tools/drawTools/operation/vabstractoperation.h"
#include "../../tools/drawTools/operation/slashspread.h"
#include "dialogtool.h"

class QComboBox;
class QSpinBox;

namespace Ui
{
    class DialogSlashSpread;
}

class DialogSlashSpread : public DialogTool
{
    Q_OBJECT
public:
    explicit            DialogSlashSpread(const VContainer *data, const quint32 &toolId, QWidget *parent = nullptr);
    virtual            ~DialogSlashSpread();

    quint32             getHingeId() const;
    void                setHingeId(quint32 value);
    quint32             getEdgeStartId() const;
    void                setEdgeStartId(quint32 value);
    quint32             getEdgeEndId() const;
    void                setEdgeEndId(quint32 value);

    int                 getSlashes() const;
    void                setSlashes(int value);
    bool                isPercent() const;
    void                setPercent(bool value);
    SlashSpread::Fixed  getFixed() const;
    void                setFixed(SlashSpread::Fixed value);

    QString             getSpread() const;
    void                setSpread(const QString &value);

    QString             getSuffix() const;
    void                setSuffix(const QString &value);

    QVector<SourceItem> getSourceObjects() const;
    void                setSourceObjects(const QVector<SourceItem> &value);

    virtual void        ShowDialog(bool click) override;

public slots:
    virtual void        ChosenObject(quint32 id, const SceneObject &type) override;
    virtual void        SelectedObject(bool selected, quint32 id, quint32 tool) override;

private slots:
    void                spreadChanged();
    void                editSpreadFormula();
    void                suffixChanged();
    void                optionsChanged();

protected:
    virtual void        CheckState() final;
    virtual void        ShowVisualization() override;
    virtual void        SaveData() override;
    virtual void        closeEvent(QCloseEvent *event) override;

private:
    Q_DISABLE_COPY(DialogSlashSpread)
    Ui::DialogSlashSpread *ui;
    QSpinBox           *m_slashesBox;
    QComboBox          *m_typeBox;
    bool                m_spreadFlag;
    bool                m_pointsFlag;
    QTimer             *m_spreadTimer;
    QString             m_spreadFormula;
    QVector<SourceItem> m_objects;
    bool                m_stage1;
    int                 m_pick;
    QString             m_suffix;

    void                evaluateSpread();
    void                updateVisualization();
    void                setPoint(QComboBox *box, quint32 value);
    QString             unitText() const;
};

#endif // DIALOGSLASHSPREAD_H
