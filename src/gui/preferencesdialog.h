#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>

class PreferencesDialog : public QDialog
{
    Q_OBJECT
public:
    PreferencesDialog(QWidget* parent);
    ~PreferencesDialog();

signals:
    void frameSizeChanged(int value);
    void fontSizeChanged(int value);

private slots:
    void styleChanged(const QString &text);
    void frameSizeChange(int value);
    void fontSizeChange(int value);

private:
    QComboBox* m_styleBox;
    QSlider*   m_frameSize;
    QSpinBox*  mFontSize;
};

#endif // PREFERENCESDIALOG_H
