/******************************************************************************
 **  @file   prova3d_setup_dialog.h
 **  @brief  Prova 3D: the window where the pieces and the seams of the pattern are set up.
 **  This file is part of Seamly2D, released under the GNU General Public License v3.
 *****************************************************************************/

#ifndef PROVA3D_SETUP_DIALOG_H
#define PROVA3D_SETUP_DIALOG_H

#include <QDialog>

#include "prova3d_setup.h"

class QTableWidget;
class QComboBox;

class Prova3dSetupDialog : public QDialog
{
    Q_OBJECT
public:
    Prova3dSetupDialog(const prova3d::Setup &setup, const VContainer *data, QWidget *parent = nullptr);
    prova3d::Setup setup() const;

private slots:
    void addSeam();
    void removeSeam();

private:
    void addPieceRow(const prova3d::PieceSetup &p);
    void addSeamRow(const prova3d::SeamSetup &s);
    void fillNodes(QComboBox *box, quint32 pieceId, quint32 current) const;
    QComboBox *pieceCombo(quint32 current) const;

    const VContainer *m_data;
    QVector<quint32> m_pieceIds;
    QTableWidget *m_pieces{nullptr};
    QTableWidget *m_seams{nullptr};
};

#endif // PROVA3D_SETUP_DIALOG_H
