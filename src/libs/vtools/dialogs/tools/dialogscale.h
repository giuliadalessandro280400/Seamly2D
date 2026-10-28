/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogscale.h
 **  @brief  Scale operation: scales selected objects relative to an origin
 **          point by independent X and Y factors. Based on the Rotation tool.
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

#ifndef DIALOGSCALE_H
#define DIALOGSCALE_H

#include <qcompilerdetection.h>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include "../vmisc/def.h"
#include "../../tools/drawTools/operation/vabstractoperation.h"
#include "dialogtool.h"

class QLabel;
class QPlainTextEdit;

namespace Ui
{
    class DialogScale;
}

class DialogScale : public DialogTool
{
    Q_OBJECT
public:
    explicit            DialogScale(const VContainer *data, const quint32 &toolId, QWidget *parent = nullptr);
    virtual            ~DialogScale();

    quint32             getOriginPointId() const;
    void                setOriginPointId(const quint32 &value);

    QString             getXScale() const;
    void                setXScale(const QString &value);

    QString             getYScale() const;
    void                setYScale(const QString &value);

    QString             getSuffix() const;
    void                setSuffix(const QString &value);

    QVector<SourceItem> getSourceObjects() const;
    void                setSourceObjects(const QVector<SourceItem> &value);

    virtual void        ShowDialog(bool click) override;

public slots:
    virtual void        ChosenObject(quint32 id, const SceneObject &type) override;
    virtual void        SelectedObject(bool selected, quint32 id, quint32 tool) override;

private slots:
    void                xScaleChanged();
    void                yScaleChanged();
    void                editXScaleFormula();
    void                editYScaleFormula();
    void                proportionalChanged(bool checked);
    void                suffixChanged();
    void                pointChanged();

protected:
    virtual void        CheckState() final;
    virtual void        ShowVisualization() override;
    /** @brief SaveData Put dialog data in local variables */
    virtual void        SaveData() override;
    virtual void        closeEvent(QCloseEvent *event) override;

private:
    Q_DISABLE_COPY(DialogScale)
    Ui::DialogScale    *ui;
    bool                m_xScaleFlag;
    bool                m_yScaleFlag;
    QTimer             *m_xScaleTimer;
    QTimer             *m_yScaleTimer;
    QString             m_xScaleFormula;
    QString             m_yScaleFormula;
    QVector<SourceItem> m_objects;
    bool                m_stage1;
    QString             m_suffix;

    void                evaluateXScale();
    void                evaluateYScale();
    QString             editFormula(const QString &title, const QString &formula);
    void                updateVisualization();
};

#endif // DIALOGSCALE_H
