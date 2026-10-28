/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogscale.cpp
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

#include "dialogscale.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointF>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <Qt>

#include "../../visualization/visualization.h"
#include "../../visualization/line/operation/vistoolscale.h"
#include "../../tools/drawTools/operation/vtoolscale.h"
#include "../ifc/xml/vabstractpattern.h"
#include "../ifc/xml/vdomdocument.h"
#include "../qmuparser/qmudef.h"
#include "../support/edit_formula_dialog.h"
#include "../vgeometry/vpointf.h"
#include "../vmisc/vabstractapplication.h"
#include "../vmisc/vcommonsettings.h"
#include "../vpatterndb/vcontainer.h"
#include "../vpatterndb/vtranslatevars.h"
#include "../vwidgets/vabstractmainwindow.h"
#include "../vwidgets/vmaingraphicsscene.h"
#include "ui_dialogscale.h"

//---------------------------------------------------------------------------------------------------------------------
DialogScale::DialogScale(const VContainer *data, const quint32 &toolId, QWidget *parent)
    : DialogTool(data, toolId, parent)
    , ui(new Ui::DialogScale)
    , m_xScaleFlag(false)
    , m_yScaleFlag(false)
    , m_xScaleTimer(nullptr)
    , m_yScaleTimer(nullptr)
    , m_xScaleFormula()
    , m_yScaleFormula()
    , m_objects()
    , m_stage1(true)
    , m_suffix()
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(QIcon(":/toolicon/32x32/scale.png"));

    // Set the position that the dialog opens based on user preference.
    setDialogPosition();

    ui->xScale_PlainTextEdit->installEventFilter(this);
    ui->yScale_PlainTextEdit->installEventFilter(this);
    ui->suffix_LineEdit->setText(qApp->getCurrentDocument()->generateSuffix(QString()));

    m_xScaleTimer = new QTimer(this);
    connect(m_xScaleTimer, &QTimer::timeout, this, &DialogScale::evaluateXScale);

    m_yScaleTimer = new QTimer(this);
    connect(m_yScaleTimer, &QTimer::timeout, this, &DialogScale::evaluateYScale);

    initializeOkCancelApply(ui);

    fillComboBoxPoints(ui->originPoint_ComboBox);

    flagName = true;
    CheckState();

    connect(ui->suffix_LineEdit,        &QLineEdit::textChanged,        this, &DialogScale::suffixChanged);
    connect(ui->xFormula_ToolButton,    &QPushButton::clicked,          this, &DialogScale::editXScaleFormula);
    connect(ui->yFormula_ToolButton,    &QPushButton::clicked,          this, &DialogScale::editYScaleFormula);
    connect(ui->xScale_PlainTextEdit,   &QPlainTextEdit::textChanged,   this, &DialogScale::xScaleChanged);
    connect(ui->yScale_PlainTextEdit,   &QPlainTextEdit::textChanged,   this, &DialogScale::yScaleChanged);
    connect(ui->proportional_CheckBox,  &QCheckBox::toggled,            this, &DialogScale::proportionalChanged);
    connect(ui->originPoint_ComboBox,   &QComboBox::currentTextChanged, this, &DialogScale::pointChanged);

    vis = new VisToolScale(data);

    setXScale(QStringLiteral("1"));
    setYScale(QStringLiteral("1"));

    ui->xScale_PlainTextEdit->setFocus();
}

//---------------------------------------------------------------------------------------------------------------------
DialogScale::~DialogScale()
{
    delete ui;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 DialogScale::getOriginPointId() const
{
    return getCurrentObjectId(ui->originPoint_ComboBox);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::setOriginPointId(const quint32 &value)
{
    changeCurrentData(ui->originPoint_ComboBox, value);
    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    SCASSERT(operation != nullptr)
    operation->setOriginPointId(value);
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogScale::getXScale() const
{
    return qApp->translateVariables()->TryFormulaFromUser(m_xScaleFormula, qApp->Settings()->getOsSeparator());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::setXScale(const QString &value)
{
    m_xScaleFormula = qApp->translateVariables()->FormulaToUser(value, qApp->Settings()->getOsSeparator());
    ui->xScale_PlainTextEdit->setPlainText(m_xScaleFormula);

    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    SCASSERT(operation != nullptr)
    operation->setXScale(m_xScaleFormula);
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogScale::getYScale() const
{
    return qApp->translateVariables()->TryFormulaFromUser(m_yScaleFormula, qApp->Settings()->getOsSeparator());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::setYScale(const QString &value)
{
    m_yScaleFormula = qApp->translateVariables()->FormulaToUser(value, qApp->Settings()->getOsSeparator());
    ui->yScale_PlainTextEdit->setPlainText(m_yScaleFormula);

    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    SCASSERT(operation != nullptr)
    operation->setYScale(m_yScaleFormula);

    // An existing tool with equal formulas reopens in proportional mode.
    ui->proportional_CheckBox->blockSignals(true);
    ui->proportional_CheckBox->setChecked(m_yScaleFormula == m_xScaleFormula);
    ui->yScale_PlainTextEdit->setEnabled(m_yScaleFormula != m_xScaleFormula);
    ui->yFormula_ToolButton->setEnabled(m_yScaleFormula != m_xScaleFormula);
    ui->proportional_CheckBox->blockSignals(false);
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogScale::getSuffix() const
{
    return m_suffix;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::setSuffix(const QString &value)
{
    m_suffix = value;
    ui->suffix_LineEdit->setText(value);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<SourceItem> DialogScale::getSourceObjects() const
{
    return m_objects;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::setSourceObjects(const QVector<SourceItem> &value)
{
    m_objects = value;

    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    SCASSERT(operation != nullptr)
    operation->setObjects(sourceToObjects(m_objects));
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::ShowDialog(bool click)
{
    if (m_stage1 && not click)
    {
        if (m_objects.isEmpty())
        {
            return;
        }

        m_stage1 = false;

        VMainGraphicsScene *scene = qobject_cast<VMainGraphicsScene *>(qApp->getCurrentScene());
        SCASSERT(scene != nullptr)
        scene->clearSelection();

        VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
        SCASSERT(operation != nullptr)
        operation->setObjects(sourceToObjects(m_objects));
        operation->VisualMode();

        scene->ToggleArcSelection(false);
        scene->ToggleElArcSelection(false);
        scene->ToggleSplineSelection(false);
        scene->ToggleSplinePathSelection(false);

        scene->ToggleArcHover(false);
        scene->ToggleElArcHover(false);
        scene->ToggleSplineHover(false);
        scene->ToggleSplinePathHover(false);

        qApp->getSceneView()->allowRubberBand(false);

        emit ToolTip(tr("Select the origin point of the scaling"));
    }
    else if (not m_stage1 && prepare && click)
    {
        CheckState();
        emit ToolTip("");
        show();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::ChosenObject(quint32 id, const SceneObject &type)
{
    if (not m_stage1 && not prepare)// After first choose we ignore all objects
    {
        if (type == SceneObject::Point)
        {
            VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
            SCASSERT(operation != nullptr)

            auto object = std::find_if(m_objects.begin(), m_objects.end(),
                                       [id](const SourceItem &item) { return item.id == id; });

            if (object != m_objects.end())
            {
                if (m_objects.size() > 1)
                {
                    // The origin point does not move when scaled, so silently remove it from the list.
                    m_objects.erase(object);
                    operation->setObjects(sourceToObjects(m_objects));
                }
                else
                {
                    emit ToolTip(tr("Select an origin point that is not part of the list of objects"));
                    return;
                }
            }

            if (SetObject(id, ui->originPoint_ComboBox, ""))
            {
                VAbstractMainWindow *window = qobject_cast<VAbstractMainWindow *>(qApp->getMainWindow());
                SCASSERT(window != nullptr)
                connect(operation, &Visualization::ToolTip, window, &VAbstractMainWindow::setStatusMessage);

                operation->setOriginPointId(id);
                operation->RefreshGeometry();

                prepare = true;
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::SelectedObject(bool selected, quint32 id, quint32 tool)
{
    Q_UNUSED(tool)
    if (m_stage1)
    {
        auto object = std::find_if(m_objects.begin(), m_objects.end(),
                                   [id](const SourceItem &item) { return item.id == id; });

        if (selected)
        {
            if (object == m_objects.cend())
            {
                SourceItem item;
                item.id = id;
                m_objects.append(item);
            }
        }
        else
        {
            if (object != m_objects.end())
            {
                m_objects.erase(object);
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::xScaleChanged()
{
    if (ui->proportional_CheckBox->isChecked())
    {
        ui->yScale_PlainTextEdit->setPlainText(ui->xScale_PlainTextEdit->toPlainText());
    }
    labelEditFormula = ui->editXScale_Label;
    labelResultCalculation = ui->resultXScale_Label;
    formulaValueChanged(m_xScaleFlag, ui->xScale_PlainTextEdit, m_xScaleTimer, VToolScale::scalePostfix());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::yScaleChanged()
{
    labelEditFormula = ui->editYScale_Label;
    labelResultCalculation = ui->resultYScale_Label;
    formulaValueChanged(m_yScaleFlag, ui->yScale_PlainTextEdit, m_yScaleTimer, VToolScale::scalePostfix());
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogScale::editFormula(const QString &title, const QString &formula)
{
    QString result = formula;
    EditFormulaDialog *dialog = new EditFormulaDialog(data, toolId, ToolDialog, this);
    dialog->setWindowTitle(title);
    dialog->SetFormula(formula);
    dialog->setPostfix(VToolScale::scalePostfix());
    if (dialog->exec() == QDialog::Accepted)
    {
        result = dialog->GetFormula();
    }
    delete dialog;
    return result;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::editXScaleFormula()
{
    const QString formula = editFormula(tr("Edit horizontal scale"), getXScale());
    m_xScaleFormula = qApp->translateVariables()->FormulaToUser(formula, qApp->Settings()->getOsSeparator());
    ui->xScale_PlainTextEdit->setPlainText(m_xScaleFormula);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::editYScaleFormula()
{
    const QString formula = editFormula(tr("Edit vertical scale"), getYScale());
    m_yScaleFormula = qApp->translateVariables()->FormulaToUser(formula, qApp->Settings()->getOsSeparator());
    ui->yScale_PlainTextEdit->setPlainText(m_yScaleFormula);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::proportionalChanged(bool checked)
{
    ui->yScale_PlainTextEdit->setEnabled(not checked);
    ui->yFormula_ToolButton->setEnabled(not checked);
    if (checked)
    {
        ui->yScale_PlainTextEdit->setPlainText(ui->xScale_PlainTextEdit->toPlainText());
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::suffixChanged()
{
    QLineEdit* edit = qobject_cast<QLineEdit*>(sender());
    if (edit)
    {
        const QString suffix = edit->text();
        if (suffix.isEmpty())
        {
            flagName = false;
            ChangeColor(ui->suffix_Label, Qt::red);
            CheckState();
            return;
        }
        else
        {
            if (m_suffix != suffix)
            {
                QRegularExpression rx(NameRegExp());
                const QStringList uniqueNames = VContainer::AllUniqueNames();
                for (int i=0; i < uniqueNames.size(); ++i)
                {
                    const QString name = uniqueNames.at(i) + suffix;
                    if (not rx.match(name).hasMatch() || not data->IsUnique(name))
                    {
                        flagName = false;
                        ChangeColor(ui->suffix_Label, Qt::red);
                        CheckState();
                        return;
                    }
                }
            }
        }

        flagName = true;
        ChangeColor(ui->suffix_Label, okColor);
    }
    CheckState();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::CheckState()
{
    SCASSERT(ok_Button != nullptr)
    ok_Button->setEnabled(m_xScaleFlag && m_yScaleFlag && flagName && flagError);
    SCASSERT(apply_Button != nullptr)
    apply_Button->setEnabled(ok_Button->isEnabled());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::ShowVisualization()
{
    AddVisualization<VisToolScale>();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::SaveData()
{
    m_suffix = ui->suffix_LineEdit->text();

    m_xScaleFormula = ui->xScale_PlainTextEdit->toPlainText();
    m_xScaleFormula.replace("\n", " ");

    m_yScaleFormula = ui->yScale_PlainTextEdit->toPlainText();
    m_yScaleFormula.replace("\n", " ");

    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    SCASSERT(operation != nullptr)

    operation->setObjects(sourceToObjects(m_objects));
    operation->setOriginPointId(getOriginPointId());
    operation->setXScale(m_xScaleFormula);
    operation->setYScale(m_yScaleFormula);
    operation->RefreshGeometry();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::closeEvent(QCloseEvent *event)
{
    ui->xScale_PlainTextEdit->blockSignals(true);
    ui->yScale_PlainTextEdit->blockSignals(true);
    DialogTool::closeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::pointChanged()
{
    quint32 id = getCurrentObjectId(ui->originPoint_ComboBox);

    auto objectId = std::find_if(m_objects.begin(), m_objects.end(),
                                 [id](const SourceItem &item) { return item.id == id; });

    QColor color = okColor;
    if (objectId != m_objects.end())
    {
        flagError = false;
        color = errorColor;
    }
    else
    {
        flagError = true;
        color = okColor;
    }
    ChangeColor(ui->originPoint_Label, color);
    CheckState();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::evaluateXScale()
{
    labelEditFormula = ui->editXScale_Label;
    Eval(ui->xScale_PlainTextEdit->toPlainText(), m_xScaleFlag, ui->resultXScale_Label,
         VToolScale::scalePostfix(), true);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::evaluateYScale()
{
    labelEditFormula = ui->editYScale_Label;
    Eval(ui->yScale_PlainTextEdit->toPlainText(), m_yScaleFlag, ui->resultYScale_Label,
         VToolScale::scalePostfix(), true);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogScale::updateVisualization()
{
    if (not m_xScaleFlag || not m_yScaleFlag)
    {
        return;
    }

    VisToolScale *operation = qobject_cast<VisToolScale *>(vis);
    // Preview while creating (after the objects were chosen) or while editing an existing tool.
    if (operation != nullptr && (not m_stage1 || toolId != NULL_ID))
    {
        operation->setXScale(ui->xScale_PlainTextEdit->toPlainText());
        operation->setYScale(ui->yScale_PlainTextEdit->toPlainText());
        operation->RefreshGeometry();
    }
}
