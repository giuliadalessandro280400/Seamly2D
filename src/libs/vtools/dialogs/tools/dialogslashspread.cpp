/***************************************************************************
 *                                                                         *
 *   Copyright (C) 2026  Seamly2D project                                  *
 *                                                                         *
 *   https://github.com/fashionfreedom/seamly2d                            *
 *                                                                         *
 ***************************************************************************
 **
 **  @file   dialogslashspread.cpp
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

#include "dialogslashspread.h"

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QSharedPointer>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <Qt>

#include "../../visualization/visualization.h"
#include "../../visualization/line/operation/vistoolslashspread.h"
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
#include "ui_dialogslashspread.h"

//---------------------------------------------------------------------------------------------------------------------
DialogSlashSpread::DialogSlashSpread(const VContainer *data, const quint32 &toolId, QWidget *parent)
    : DialogTool(data, toolId, parent)
    , ui(new Ui::DialogSlashSpread)
    , m_slashesBox(nullptr)
    , m_typeBox(nullptr)
    , m_spreadFlag(false)
    , m_pointsFlag(false)
    , m_spreadTimer(nullptr)
    , m_spreadFormula()
    , m_objects()
    , m_stage1(true)
    , m_pick(0)
    , m_suffix()
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(QIcon(":/toolicon/32x32/slash_spread.png"));

    setDialogPosition();

    ui->plainTextEditFormula->installEventFilter(this);
    ui->suffix_LineEdit->setText(qApp->getCurrentDocument()->generateSuffix(QString()));

    m_spreadTimer = new QTimer(this);
    connect(m_spreadTimer, &QTimer::timeout, this, &DialogSlashSpread::evaluateSpread);

    initializeOkCancelApply(ui);

    for (QComboBox *box : {ui->hinge_ComboBox, ui->edgeStart_ComboBox, ui->edgeEnd_ComboBox})
    {
        fillComboBoxPoints(box);
        connect(box, &QComboBox::currentTextChanged, this, &DialogSlashSpread::optionsChanged);
    }

    ui->fixedSide_ComboBox->addItem(tr("The part at the first point of the edge"),
                                    static_cast<int>(SlashSpread::Fixed::Start));
    ui->fixedSide_ComboBox->addItem(tr("The part at the last point of the edge"),
                                    static_cast<int>(SlashSpread::Fixed::End));
    ui->fixedSide_ComboBox->addItem(tr("The middle: both sides open"), static_cast<int>(SlashSpread::Fixed::Both));
    connect(ui->fixedSide_ComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &DialogSlashSpread::optionsChanged);

    // How many slashes and how the spread is measured
    m_slashesBox = new QSpinBox(this);
    m_slashesBox->setRange(1, 30);
    m_slashesBox->setValue(3);
    m_typeBox = new QComboBox(this);
    m_typeBox->addItem(tr("Length added to the edge"));
    m_typeBox->addItem(tr("Percentage of the edge"));
    const int row = ui->formLayout->rowCount() - 2; // before "Fixed part" and "Suffix"
    ui->formLayout->insertRow(row, tr("Slashes:"), m_slashesBox);
    ui->formLayout->insertRow(row + 1, tr("Spread measured as:"), m_typeBox);
    for (QWidget *field : ui->groupBox->findChildren<QWidget *>())
    {
        if (qobject_cast<QComboBox *>(field) || qobject_cast<QSpinBox *>(field) || qobject_cast<QLineEdit *>(field))
        {
            field->setMinimumHeight(26);
        }
    }
    ui->groupBox->setMinimumHeight(ui->groupBox->sizeHint().height() + 16);
    setMinimumSize(420, 480);
    resize(420, 480);
    connect(m_slashesBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &DialogSlashSpread::optionsChanged);
    connect(m_typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]()
    {
        evaluateSpread();
        optionsChanged();
    });

    flagName = true;
    CheckState();

    connect(ui->suffix_LineEdit,      &QLineEdit::textChanged,      this, &DialogSlashSpread::suffixChanged);
    connect(ui->formula_ToolButton,   &QPushButton::clicked,        this, &DialogSlashSpread::editSpreadFormula);
    connect(ui->plainTextEditFormula, &QPlainTextEdit::textChanged, this, &DialogSlashSpread::spreadChanged);

    vis = new VisToolSlashSpread(data);

    setSpread(QStringLiteral("2"));
}

//---------------------------------------------------------------------------------------------------------------------
DialogSlashSpread::~DialogSlashSpread()
{
    delete ui;
}

//---------------------------------------------------------------------------------------------------------------------
quint32 DialogSlashSpread::getHingeId() const     { return getCurrentObjectId(ui->hinge_ComboBox); }
quint32 DialogSlashSpread::getEdgeStartId() const { return getCurrentObjectId(ui->edgeStart_ComboBox); }
quint32 DialogSlashSpread::getEdgeEndId() const   { return getCurrentObjectId(ui->edgeEnd_ComboBox); }
void DialogSlashSpread::setHingeId(quint32 value)     { setPoint(ui->hinge_ComboBox, value); }
void DialogSlashSpread::setEdgeStartId(quint32 value) { setPoint(ui->edgeStart_ComboBox, value); }
void DialogSlashSpread::setEdgeEndId(quint32 value)   { setPoint(ui->edgeEnd_ComboBox, value); }

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::setPoint(QComboBox *box, quint32 value)
{
    changeCurrentData(box, value);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
int DialogSlashSpread::getSlashes() const { return m_slashesBox->value(); }
void DialogSlashSpread::setSlashes(int value) { m_slashesBox->setValue(value); }
bool DialogSlashSpread::isPercent() const { return m_typeBox->currentIndex() == 1; }
void DialogSlashSpread::setPercent(bool value) { m_typeBox->setCurrentIndex(value ? 1 : 0); }

//---------------------------------------------------------------------------------------------------------------------
SlashSpread::Fixed DialogSlashSpread::getFixed() const
{
    return static_cast<SlashSpread::Fixed>(ui->fixedSide_ComboBox->currentData().toInt());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::setFixed(SlashSpread::Fixed value)
{
    const int index = ui->fixedSide_ComboBox->findData(static_cast<int>(value));
    if (index >= 0)
    {
        ui->fixedSide_ComboBox->setCurrentIndex(index);
    }
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogSlashSpread::getSpread() const
{
    return qApp->translateVariables()->TryFormulaFromUser(m_spreadFormula, qApp->Settings()->getOsSeparator());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::setSpread(const QString &value)
{
    m_spreadFormula = qApp->translateVariables()->FormulaToUser(value, qApp->Settings()->getOsSeparator());
    ui->plainTextEditFormula->setPlainText(m_spreadFormula);

    VisToolSlashSpread *operation = qobject_cast<VisToolSlashSpread *>(vis);
    SCASSERT(operation != nullptr)
    operation->setSpread(m_spreadFormula);
}

//---------------------------------------------------------------------------------------------------------------------
QString DialogSlashSpread::getSuffix() const
{
    return m_suffix;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::setSuffix(const QString &value)
{
    m_suffix = value;
    ui->suffix_LineEdit->setText(value);
}

//---------------------------------------------------------------------------------------------------------------------
QVector<SourceItem> DialogSlashSpread::getSourceObjects() const
{
    return m_objects;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::setSourceObjects(const QVector<SourceItem> &value)
{
    m_objects = value;

    VisToolSlashSpread *operation = qobject_cast<VisToolSlashSpread *>(vis);
    SCASSERT(operation != nullptr)
    operation->setObjects(sourceToObjects(m_objects));
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::ShowDialog(bool click)
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

        VisToolSlashSpread *operation = qobject_cast<VisToolSlashSpread *>(vis);
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

        emit ToolTip(tr("Select the hinge point (where the slashes end)"));
    }
    else if (not m_stage1 && prepare && click)
    {
        CheckState();
        emit ToolTip("");
        show();
    }
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::ChosenObject(quint32 id, const SceneObject &type)
{
    if (m_stage1 || prepare || type != SceneObject::Point)
    {
        return;
    }

    VisToolSlashSpread *operation = qobject_cast<VisToolSlashSpread *>(vis);
    SCASSERT(operation != nullptr)

    switch (m_pick)
    {
        case 0:
            if (SetObject(id, ui->hinge_ComboBox, tr("Select the first point of the edge to open")))
            {
                operation->setHingeId(id);
                ++m_pick;
            }
            break;
        case 1:
            if (id == getHingeId())
            {
                emit ToolTip(tr("Select a point different from the hinge"));
                return;
            }
            if (SetObject(id, ui->edgeStart_ComboBox, tr("Select the last point of the edge to open")))
            {
                operation->setEdgeStartId(id);
                ++m_pick;
            }
            break;
        case 2:
            if (id == getHingeId() || id == getEdgeStartId())
            {
                emit ToolTip(tr("Select a point different from the hinge and the first point"));
                return;
            }
            if (SetObject(id, ui->edgeEnd_ComboBox, QString()))
            {
                operation->setEdgeEndId(id);
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
void DialogSlashSpread::SelectedObject(bool selected, quint32 id, quint32 tool)
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
QString DialogSlashSpread::unitText() const
{
    return isPercent() ? QStringLiteral("%") : UnitsToStr(qApp->patternUnit(), true);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::spreadChanged()
{
    labelEditFormula = ui->editSpread_Label;
    labelResultCalculation = ui->resultSpread_Label;
    formulaValueChanged(m_spreadFlag, ui->plainTextEditFormula, m_spreadTimer, unitText());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::editSpreadFormula()
{
    EditFormulaDialog *dialog = new EditFormulaDialog(data, toolId, ToolDialog, this);
    dialog->setWindowTitle(tr("Edit spread"));
    dialog->SetFormula(getSpread());
    dialog->setPostfix(unitText());
    if (dialog->exec() == QDialog::Accepted)
    {
        setSpread(dialog->GetFormula());
    }
    delete dialog;
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::suffixChanged()
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
void DialogSlashSpread::optionsChanged()
{
    const quint32 hinge = getHingeId();
    const bool edgeOk = getEdgeStartId() != getEdgeEndId() && getEdgeStartId() != hinge && getEdgeEndId() != hinge;
    ChangeColor(ui->edgeEnd_Label, edgeOk ? okColor : errorColor);
    m_pointsFlag = edgeOk;
    updateVisualization();
    CheckState();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::CheckState()
{
    SCASSERT(ok_Button != nullptr)
    ok_Button->setEnabled(m_spreadFlag && m_pointsFlag && flagName);
    SCASSERT(apply_Button != nullptr)
    apply_Button->setEnabled(ok_Button->isEnabled());
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::ShowVisualization()
{
    AddVisualization<VisToolSlashSpread>();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::SaveData()
{
    m_suffix = ui->suffix_LineEdit->text();

    m_spreadFormula = ui->plainTextEditFormula->toPlainText();
    m_spreadFormula.replace("\n", " ");

    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::closeEvent(QCloseEvent *event)
{
    ui->plainTextEditFormula->blockSignals(true);
    DialogTool::closeEvent(event);
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::evaluateSpread()
{
    labelEditFormula = ui->editSpread_Label;
    Eval(ui->plainTextEditFormula->toPlainText(), m_spreadFlag, ui->resultSpread_Label, unitText(), false);
    updateVisualization();
}

//---------------------------------------------------------------------------------------------------------------------
void DialogSlashSpread::updateVisualization()
{
    VisToolSlashSpread *operation = qobject_cast<VisToolSlashSpread *>(vis);
    // Preview while creating (after the points were chosen) or while editing an existing tool.
    if (operation == nullptr || m_slashesBox == nullptr || (m_stage1 && toolId == NULL_ID)
        || (m_pick < 3 && toolId == NULL_ID))
    {
        return;
    }

    operation->setObjects(sourceToObjects(m_objects));
    operation->setHingeId(getHingeId());
    operation->setEdgeStartId(getEdgeStartId());
    operation->setEdgeEndId(getEdgeEndId());
    operation->setSlashes(getSlashes());
    operation->setPercent(isPercent());
    operation->setFixed(getFixed());
    if (m_spreadFlag)
    {
        operation->setSpread(ui->plainTextEditFormula->toPlainText());
    }
    operation->RefreshGeometry();
}
