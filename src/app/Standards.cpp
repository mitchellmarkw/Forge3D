// SPDX-License-Identifier: GPL-3.0-or-later
#include "Standards.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <stdexcept>

namespace forge::app::standardsfile {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

}  // namespace

QString path()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
           QStringLiteral("/Forge3D/Joint Standards.json");
}

std::vector<standards::Standard> load()
{
    QFile file(path());
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        throw std::invalid_argument("The joint standards file can't be opened.");
    return standards::fromJson(file.readAll().toStdString());
}

void save(const std::vector<standards::Standard>& list)
{
    QDir().mkpath(QFileInfo(path()).absolutePath());
    QSaveFile file(path());
    if (!file.open(QIODevice::WriteOnly))
        throw std::runtime_error("The joint standards file can't be written.");
    const std::string text = standards::toJson(list);
    file.write(text.data(), qint64(text.size()));
    if (!file.commit())
        throw std::runtime_error("The joint standards file can't be written.");
}

QString activeName()
{
    return preferences().value(QStringLiteral("standards/active")).toString();
}

void setActiveName(const QString& name)
{
    preferences().setValue(QStringLiteral("standards/active"), name);
}

std::optional<standards::Standard> active()
{
    const QString name = activeName();
    if (name.isEmpty())
        return std::nullopt;
    try {
        for (const auto& s : load())
            if (QString::fromStdString(s.name) == name)
                return s;
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

}  // namespace forge::app::standardsfile

namespace forge::app {

StandardRow::StandardRow(QWidget* parent) : QWidget(parent)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    m_label = new QLabel;
    column->addWidget(m_label);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_joints = new QComboBox;
    m_joints->setToolTip(tr("Start from one of the joint standard's joints: its sizes come over (the printer and "
                            "fit stay as they are)"));
    m_save = new QPushButton(tr("Save to Standard..."));
    m_save->setToolTip(tr("Keep these sizes as a named joint in the joint standard in use (Tools → Joint "
                          "Standards), for the next figure in the line"));
    row->addWidget(m_joints, 1);
    row->addWidget(m_save);
    column->addLayout(row);
    connect(m_joints, &QComboBox::activated, this, [this](int i) {
        if (i > 0)
            emit chosen(i - 1);
    });
    connect(m_save, &QPushButton::clicked, this, &StandardRow::saveRequested);
    setJoints({}, {});
}

void StandardRow::setJoints(const QString& standard, const QStringList& names)
{
    m_label->setText(standard.isEmpty() ? tr("Joint standard:") : tr("From the joint standard '%1':").arg(standard));
    m_joints->clear();
    m_joints->addItem(standard.isEmpty() ? tr("None in use") : names.isEmpty() ? tr("No joints of this kind yet")
                                                                              : tr("Choose a joint..."));
    m_joints->addItems(names);
    m_joints->setEnabled(!names.isEmpty());
}

}  // namespace forge::app
