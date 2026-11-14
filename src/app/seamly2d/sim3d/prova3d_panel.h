/******************************************************************************
 **  @file   prova3d_panel.h
 **  @brief  Prova 3D: the panel beside the pattern (view of the form, the fitting and the drop).
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#ifndef PROVA3D_PANEL_H
#define PROVA3D_PANEL_H

#include <QPointer>
#include <QWidget>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "prova3d_engine.h"

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QThread;
class VAbstractPattern;
class VContainer;

/// Draws the form and the garment, paper on paper. Drag to turn, wheel to zoom.
class Prova3dView : public QWidget
{
    Q_OBJECT
public:
    explicit Prova3dView(QWidget *parent = nullptr);

    void setForm(const sim::DressForm *form);
    void setGarment(std::shared_ptr<sim::Garment> garment, const std::vector<QString> &seamNames);
    QSize sizeHint() const override { return {420, 560}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    std::vector<sim::V3> m_formVerts;
    std::vector<std::array<int, 3>> m_formTris;
    std::shared_ptr<sim::Garment> m_garment;
    std::vector<QString> m_seamNames;
    double m_yaw{0.35};
    double m_pitch{0.08};
    double m_zoom{1.0};
    QPoint m_lastPos;
};

/// The whole panel: title, view, material, buttons and the notes on the seams.
class Prova3dPanel : public QWidget
{
    Q_OBJECT
public:
    explicit Prova3dPanel(QWidget *parent = nullptr);
    ~Prova3dPanel() override;

    /// the pattern whose pieces go on the form
    void setPattern(VAbstractPattern *doc, const VContainer *data);

    // the look of the panel, shared with the setup window
    static QString styleSheetText();
    static QFont titleFont();
    static QFont noteFont();
    static QFont capsFont();
    static QLabel *sectionLabel(const QString &text, QWidget *parent);

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void phaseFinished(const QString &summary);

private slots:
    void fitInPaper();
    void drop();
    void restart();
    void onPhaseFinished(const QString &summary);
    void openSetup();
    void setSource(bool pattern);

private:
    void runPhase(int phase);
    void setBusy(bool busy);
    void refreshNotes();
    sim::Material fabric() const;

    Prova3dView *m_view{nullptr};
    QPushButton *m_paperButton{nullptr};
    QPushButton *m_dropButton{nullptr};
    QPushButton *m_restartButton{nullptr};
    QDoubleSpinBox *m_width{nullptr};
    QDoubleSpinBox *m_warp{nullptr};
    QDoubleSpinBox *m_weft{nullptr};
    QDoubleSpinBox *m_lock{nullptr};
    QDoubleSpinBox *m_weight{nullptr};
    QLabel *m_status{nullptr};
    QLabel *m_notes{nullptr};
    QPushButton *m_patternSource{nullptr};
    QPushButton *m_demoSource{nullptr};
    QPushButton *m_setupButton{nullptr};
    QWidget *m_demoBox{nullptr};
    VAbstractPattern *m_doc{nullptr};
    const VContainer *m_data{nullptr};
    bool m_usePattern{true};

    std::unique_ptr<sim::DressForm> m_form;
    std::shared_ptr<sim::Garment> m_garment;
    std::vector<QString> m_seamNames;
    QThread *m_worker{nullptr};
    bool m_dropped{false};
};

#endif // PROVA3D_PANEL_H
