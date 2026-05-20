#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

class AppCore;
class QEvent;
class QCheckBox;
class QComboBox;
class QLabel;

class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent, AppCore* core);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onApplyClicked();
    void onResetDefaultsClicked();
    void clearCacheClicked();
    void openCrashReportsFolderClicked();

private:
    void loadFromSettings();
    void saveFromUi();
    void refreshHintColors();
    void refreshGpuAdapterList();

    QVector<QLabel*> m_hintLabels;
    AppCore* m_core = nullptr;
    QComboBox* m_theme = nullptr;
    QComboBox* m_language = nullptr;
    QLabel* m_themeHint = nullptr;
    QLabel* m_languageHint = nullptr;
    QCheckBox* m_ignorePluginVersion = nullptr;
    QLabel* m_ignorePluginHint = nullptr;
    QCheckBox* m_traceFlow = nullptr;
    QLabel* m_traceHint = nullptr;
    QCheckBox* m_welcomeStartup = nullptr;
    QLabel* m_welcomeHint = nullptr;
    QCheckBox* m_developerMode = nullptr;
    QLabel* m_developerHint = nullptr;
    QCheckBox* m_enumVideoStartup = nullptr;
    QLabel* m_enumVideoStartupHint = nullptr;
    QCheckBox* m_virtualCamStartup = nullptr;
    QLabel* m_virtualCamStartupHint = nullptr;
    QCheckBox* m_showGpuLoad = nullptr;
    QLabel* m_showGpuLoadHint = nullptr;
    QComboBox* m_gpuAdapter = nullptr;
    QLabel* m_gpuAdapterHint = nullptr;
};

#endif
