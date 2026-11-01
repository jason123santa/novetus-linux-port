#pragma once
#include <QDialog>
#include "core/clientinfo.h"
#include "core/config.h"

class QSpinBox;
class QComboBox;
class QLineEdit;

// Port of NovetusLauncher.CustomGraphicsOptions: per-client custom graphics
// settings edited directly in the client's GlobalSettings XML. Values are
// loaded from the selected client's settings template on open and written
// back through ClientSettings::applyCustomClientSettings on close.
class GraphicsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GraphicsDialog(const Config *userConfig, QWidget *parent = nullptr);

protected:
    // FormClosing parity: save on any close (after the load check passed).
    void closeEvent(QCloseEvent *event) override;
    // QDialog::done()/reject() hide the dialog without a closeEvent; the X
    // button goes through reject(), so save there as well.
    void reject() override;

private:
    void loadValues();
    void save();

    const Config *m_user;
    ClientInfo m_info;
    QString m_clientName;
    bool m_loaded = false;

    QSpinBox *m_level = nullptr;
    QSpinBox *m_mesh = nullptr;
    QSpinBox *m_shading = nullptr;
    QComboBox *m_material = nullptr;
    QComboBox *m_aa = nullptr;
    QComboBox *m_aaSamples = nullptr;
    QComboBox *m_bevels = nullptr;
    QComboBox *m_shadows2008 = nullptr;
    QComboBox *m_shadows2007 = nullptr;
    QLineEdit *m_style = nullptr;
    QLineEdit *m_windowRes = nullptr;
    QLineEdit *m_fullscreenRes = nullptr;
    QComboBox *m_modernRes = nullptr;
};
