#include "projectpropertiesdialog.h"

#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>


ProjectPropertiesDialog::ProjectPropertiesDialog(QWidget *parent, int width, int height)
    : QDialog(parent,Qt::CustomizeWindowHint|Qt::WindowCloseButtonHint)
{
    setWindowTitle("Properties");
    QLabel* widthLabel = new QLabel(tr("Width:"));
    QLabel* heightLabel = new QLabel(tr("Height:"));
    m_widthBox = new QSpinBox();
    m_heightBox = new QSpinBox();
    m_widthBox->setMaximum(10000);
    m_heightBox->setMaximum(10000);
    m_widthBox->setMinimum(1);
    m_heightBox->setMinimum(1);
    m_widthBox->setValue(width);
    m_heightBox->setValue(height);
    QGridLayout* sizeLayout = new QGridLayout();
    sizeLayout->addWidget(widthLabel, 1, 0);
    sizeLayout->addWidget(m_widthBox, 1, 1);
    sizeLayout->addWidget(heightLabel, 2, 0);
    sizeLayout->addWidget(m_heightBox, 2, 1);

    QPushButton* okButton = new QPushButton(tr("Ok"));
    okButton->setDefault(true);
    QPushButton* cancelButton = new QPushButton(tr("Cancel"));
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    QGridLayout* layout = new QGridLayout();
    layout->addLayout(sizeLayout, 1, 0);
    layout->addLayout(buttonLayout, 2, 0);
    setLayout(layout);
    connect(okButton, SIGNAL(pressed()), this, SLOT(accept()));
    connect(cancelButton, SIGNAL(pressed()), this, SLOT(reject()));
}
