/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogalign.cpp
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

#include "dialogalign.h"

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSharedPointer>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <Qt>

#include "../../visualization/visualization.h"
#include "../../visualization/line/operation/vistoolalign.h"
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
#include "ui_dialogalign.h"

//---------------------------------------------------------------------------------------------------------------------
DialogAlign::DialogAlign(const VContainer *data, const quint32 &toolId, QWidget *parent)
    : DialogTool(data, toolId, parent)
    , ui(new Ui::DialogAlign)
    , m_angleFlag(false)
    , m_pointsFlag(false)
    , m_angleTimer(nullptr)
    , m_angleFormula()
    , m_objects()
    , m_stage1(true)
    , m_pick(0)
    , m_suffix()
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(QIcon(":/toolicon/32x32/align.png"));

    setDialogPosition();

    ui->plainTextEditFormula->installEventFilter(this);
    ui->plainTextEditFormula->setToolTip(makeAngleTooltip());
    ui->suffix_LineEdit->setText(qApp->getCurrentDocument()->generateSuffix(QString()));

    m_angleTimer = new QTimer(this);
    connect(m_angleTimer, &QTimer::timeout, this, &DialogAlign::evaluateAngle);

    initializeOkCancelApply(ui);

    for (QComboBox *box : {ui->sourcePoint_ComboBox, ui->sourceDirection_ComboBox, ui->targetPoint_ComboBox,
                           ui->targetDirection_ComboBox})
    {
        fillComboBoxPoints(box);
        connect(box, &QComboBox::currentTextChanged, this, &DialogAlign::pointsChanged);
    }

    flagName = true;
    CheckState();

    connect(ui->suffix_LineEdit,      &QLineEdit::textChanged,      this, &DialogAlign::suffixChanged);
    connect(ui->formula_ToolButton,   &QPushButton::clicked,        this, &DialogAlign::editAngleFormula);
    connect(ui->plainTextEditFormula, &QPlainTextEdit::textChanged, this, &DialogAlign::angleChanged);

    vis = new VisToolAlign(data);

    setAngle(QStringLiteral("0"));
}

//---------------------------------------------------------------------------------------------------------------------
DialogAlign::~DialogAlign()
{
    delete ui;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 DialogAlign::getSourcePointId() const     { return getCurrentObjectId(ui->sourcePoint_ComboBox); }
quint32 DialogAlign::getSourceDirectionId() const { return getCurrentObjectId(ui->sourceDirection_ComboBox); }
quint32 DialogAlign::getTargetPointId() const     { return getCurrentObjectId(ui->targetPoint_ComboBox); }
quint32 DialogAlign::getTargetDirectionId() const { return getCurrentObjectId(ui->targetDirection_ComboBox); }

void DialogAlign::setSourcePointId(quint32 value)     { setPoint(ui->sourcePoint_ComboBox, value); }
void DialogAlign::setSourceDirectionId(quint32 value) { setPoint(ui->sourceDirection_ComboBox, value); }
void DialogAlign::setTargetPointId(quint32 value)     { setPoint(ui->targetPoint_ComboBox, value); }
void DialogAlign::setTargetDirectionId(quint32 value) { setPoint(ui->targetDirection_ComboBox, value); }

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::setPoint(QComboBox *box, quint32 value)
{
    changeCurrentData(box, value);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogAlign::getAngle() const
{
    return qApp->translateVariables()->TryFormulaFromUser(m_angleFormula, qApp->Settings()->getOsSeparator());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::setAngle(const QString &value)
{
    m_angleFormula = qApp->translateVariables()->FormulaToUser(value, qApp->Settings()->getOsSeparator());
    ui->plainTextEditFormula->setPlainText(m_angleFormula);

    VisToolAlign *operation = qobject_cast<VisToolAlign *>(vis);
    SCASSERT(operation != nullptr)
    operation->setAngle(m_angleFormula);
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogAlign::getSuffix() const
{
    return m_suffix;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::setSuffix(const QString &value)
{
    m_suffix = value;
    ui->suffix_LineEdit->setText(value);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<SourceItem> DialogAlign::getSourceObjects() const
{
    return m_objects;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::setSourceObjects(const QVector<SourceItem> &value)
{
    m_objects = value;

    VisToolAlign *operation = qobject_cast<VisToolAlign *>(vis);
    SCASSERT(operation != nullptr)
    operation->setObjects(sourceToObjects(m_objects));
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::ShowDialog(bool click)
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

        VisToolAlign *operation = qobject_cast<VisToolAlign *>(vis);
        SCASSERT(operation != nullptr)
        operation->setObjects(sourceToObjects(m_objects));
        operation->VisualMode();

        VAbstractMainWindow *window = qobject_cast<VAbstractMainWindow *>(qApp->getMainWindow());
        SCASSERT(window != nullptr)
        connect(operation, &Visualization::ToolTip, window, &VAbstractMainWindow::setStatusMessage);

        scene->ToggleArcSelection(false);
        scene->ToggleElArcSelection(false);
        scene->ToggleSplineSelection(false);
        scene->ToggleSplinePathSelection(false);

        scene->ToggleArcHover(false);
        scene->ToggleElArcHover(false);
        scene->ToggleSplineHover(false);
        scene->ToggleSplinePathHover(false);

        qApp->getSceneView()->allowRubberBand(false);

        emit ToolTip(tr("Select the point of the group to move (P)"));
    }
    else if (not m_stage1 && prepare && click)
    {
        CheckState();
        emit ToolTip("");
        show();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::ChosenObject(quint32 id, const SceneObject &type)
{
    if (m_stage1 || prepare || type != SceneObject::Point)
    {
        return;
    }

    VisToolAlign *operation = qobject_cast<VisToolAlign *>(vis);
    SCASSERT(operation != nullptr)

    switch (m_pick)
    {
        case 0:
            if (SetObject(id, ui->sourcePoint_ComboBox, tr("Select the second point of the group line (A)")))
            {
                operation->setSourcePointId(id);
                ++m_pick;
            }
            break;
        case 1:
            if (id == getSourcePointId())
            {
                emit ToolTip(tr("Select a point different from P"));
                return;
            }
            if (SetObject(id, ui->sourceDirection_ComboBox, tr("Select the target point (T)")))
            {
                operation->setSourceDirectionId(id);
                ++m_pick;
            }
            break;
        case 2:
            if (SetObject(id, ui->targetPoint_ComboBox, tr("Select the second point of the target line (B)")))
            {
                operation->setTargetPointId(id);
                ++m_pick;
            }
            break;
        case 3:
            if (id == getTargetPointId())
            {
                emit ToolTip(tr("Select a point different from T"));
                return;
            }
            if (SetObject(id, ui->targetDirection_ComboBox, QString()))
            {
                operation->setTargetDirectionId(id);
                ++m_pick;
                prepare = true;
            }
            break;
        default:
            break;
    }
    operation->RefreshGeometry();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::SelectedObject(bool selected, quint32 id, quint32 tool)
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
        else if (object != m_objects.end())
        {
            m_objects.erase(object);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::angleChanged()
{
    labelEditFormula = ui->editAngle_Label;
    labelResultCalculation = ui->resultAngle_Label;
    formulaValueChanged(m_angleFlag, ui->plainTextEditFormula, m_angleTimer, degreeSymbol);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::editAngleFormula()
{
    EditFormulaDialog *dialog = new EditFormulaDialog(data, toolId, ToolDialog, this);
    dialog->setWindowTitle(tr("Edit extra angle"));
    dialog->SetFormula(getAngle());
    dialog->setPostfix(degreeSymbol);
    if (dialog->exec() == QDialog::Accepted)
    {
        setAngle(dialog->GetFormula());
    }
    delete dialog;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::suffixChanged()
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

        flagName = true;
        ChangeColor(ui->suffix_Label, okColor);
    }
    CheckState();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::pointsChanged()
{
    const bool sourceOk = getSourcePointId() != getSourceDirectionId();
    const bool targetOk = getTargetPointId() != getTargetDirectionId();
    ChangeColor(ui->sourceDirection_Label, sourceOk ? okColor : errorColor);
    ChangeColor(ui->targetDirection_Label, targetOk ? okColor : errorColor);
    m_pointsFlag = sourceOk && targetOk;
    updateVisualization();
    CheckState();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::CheckState()
{
    SCASSERT(ok_Button != nullptr)
    ok_Button->setEnabled(m_angleFlag && m_pointsFlag && flagName);
    SCASSERT(apply_Button != nullptr)
    apply_Button->setEnabled(ok_Button->isEnabled());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::ShowVisualization()
{
    AddVisualization<VisToolAlign>();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::SaveData()
{
    m_suffix = ui->suffix_LineEdit->text();

    m_angleFormula = ui->plainTextEditFormula->toPlainText();
    m_angleFormula.replace("\n", " ");

    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::closeEvent(QCloseEvent *event)
{
    ui->plainTextEditFormula->blockSignals(true);
    DialogTool::closeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::evaluateAngle()
{
    labelEditFormula = ui->editAngle_Label;
    Eval(ui->plainTextEditFormula->toPlainText(), m_angleFlag, ui->resultAngle_Label, degreeSymbol, false);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogAlign::updateVisualization()
{
    VisToolAlign *operation = qobject_cast<VisToolAlign *>(vis);
    // Preview while creating (after the points were chosen) or while editing an existing tool.
    if (operation == nullptr || (m_stage1 && toolId == NULL_ID) || (m_pick < 4 && toolId == NULL_ID))
    {
        return;
    }

    operation->setObjects(sourceToObjects(m_objects));
    operation->setSourcePointId(getSourcePointId());
    operation->setSourceDirectionId(getSourceDirectionId());
    operation->setTargetPointId(getTargetPointId());
    operation->setTargetDirectionId(getTargetDirectionId());
    if (m_angleFlag)
    {
        operation->setAngle(ui->plainTextEditFormula->toPlainText());
    }
    operation->RefreshGeometry();
}
