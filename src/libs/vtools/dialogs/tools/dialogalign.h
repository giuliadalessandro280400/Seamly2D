/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogalign.h
 **  @brief  Dialog of the Align operation.
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

#ifndef DIALOGALIGN_H
#define DIALOGALIGN_H

#include <qcompilerdetection.h>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "../vmisc/def.h"
#include "../../tools/drawTools/operation/vabstractoperation.h"
#include "dialogtool.h"

class QComboBox;

namespace Ui
{
    class DialogAlign;
}

class DialogAlign : public DialogTool
{
    Q_OBJECT
public:
    explicit            DialogAlign(const VContainer *data, const quint32 &toolId, QWidget *parent = nullptr);
    virtual            ~DialogAlign();

    quint32             getSourcePointId() const;
    void                setSourcePointId(quint32 value);
    quint32             getSourceDirectionId() const;
    void                setSourceDirectionId(quint32 value);
    quint32             getTargetPointId() const;
    void                setTargetPointId(quint32 value);
    quint32             getTargetDirectionId() const;
    void                setTargetDirectionId(quint32 value);

    QString             getAngle() const;
    void                setAngle(const QString &value);

    QString             getSuffix() const;
    void                setSuffix(const QString &value);

    QVector<SourceItem> getSourceObjects() const;
    void                setSourceObjects(const QVector<SourceItem> &value);

    virtual void        ShowDialog(bool click) override;

public slots:
    virtual void        ChosenObject(quint32 id, const SceneObject &type) override;
    virtual void        SelectedObject(bool selected, quint32 id, quint32 tool) override;

private slots:
    void                angleChanged();
    void                editAngleFormula();
    void                suffixChanged();
    void                pointsChanged();

protected:
    virtual void        CheckState() final;
    virtual void        ShowVisualization() override;
    virtual void        SaveData() override;
    virtual void        closeEvent(QCloseEvent *event) override;

private:
    Q_DISABLE_COPY(DialogAlign)
    Ui::DialogAlign    *ui;
    bool                m_angleFlag;
    bool                m_pointsFlag;
    QTimer             *m_angleTimer;
    QString             m_angleFormula;
    QVector<SourceItem> m_objects;
    bool                m_stage1;
    int                 m_pick;
    QString             m_suffix;

    void                evaluateAngle();
    void                updateVisualization();
    void                setPoint(QComboBox *box, quint32 value);
};

#endif // DIALOGALIGN_H
