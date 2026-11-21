// SPDX-License-Identifier: GPL-3.0-or-later
#include "JointStandardsDialog.h"

#include "Standards.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace forge::app {

namespace {

// A label with its combobox right under it (the label resting on top of the box).
QWidget* labelled(const QString& text, QComboBox* box)
{
    auto* holder = new QWidget;
    auto* column = new QVBoxLayout(holder);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(new QLabel(text));
    column->addWidget(box);
    return holder;
}

QString kindOf(const standards::Joint& j)
{
    if (j.socket)
        return j.socket->kind == SocketKind::Ball   ? QObject::tr("Ball socket")
               : j.socket->kind == SocketKind::Peg  ? QObject::tr("Peg socket")
                                                    : QObject::tr("Magnet pocket");
    if (j.hinge)
        return j.hinge->kind == HingeKind::Single   ? QObject::tr("Single hinge")
               : j.hinge->kind == HingeKind::Double ? QObject::tr("Double hinge")
                                                    : QObject::tr("Swivel");
    return {};
}

}  // namespace

JointStandardsDialog::JointStandardsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Joint Standards"));
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("A joint standard keeps a figure line's joint sizes by name (6 mm shoulder balls, "
                                "3 mm wrist pegs...), so every figure in the line takes the same joints. Add joints "
                                "from Quick Socket and Quick Hinge with <b>Save to Standard</b>; pick them there "
                                "to start from. The fit stays with the printer (Tolerance Test Print)."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* top = new QHBoxLayout;
    m_standard = new QComboBox;
    m_standard->setToolTip(tr("The joint standard Quick Socket, Quick Hinge and the Tolerance Test Print use"));
    top->addWidget(labelled(tr("Joint standard in use:"), m_standard), 1);
    auto* add = new QPushButton(tr("New..."));
    auto* rename = new QPushButton(tr("Rename..."));
    auto* remove = new QPushButton(tr("Delete"));
    for (QPushButton* b : {add, rename, remove})
        top->addWidget(b, 0, Qt::AlignBottom);
    layout->addLayout(top);

    m_body = new QWidget;
    auto* body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->addWidget(new QLabel(tr("Notes:")));
    m_notes = new QPlainTextEdit;
    m_notes->setPlaceholderText(tr("What it's for: the figure line, the scale, the printer..."));
    m_notes->setMaximumHeight(60);
    body->addWidget(m_notes);
    body->addWidget(new QLabel(tr("Joints:")));
    m_joints = new QTreeWidget;
    m_joints->setHeaderLabels({tr("Name"), tr("Kind"), tr("Sizes")});
    m_joints->setRootIsDecorated(false);
    m_joints->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_joints->setMinimumHeight(180);
    body->addWidget(m_joints);
    auto* jointButtons = new QHBoxLayout;
    auto* renameJoint = new QPushButton(tr("Rename Joint..."));
    auto* removeJoint = new QPushButton(tr("Remove Joint"));
    auto* up = new QPushButton(tr("Move Up"));
    auto* down = new QPushButton(tr("Move Down"));
    for (QPushButton* b : {renameJoint, removeJoint, up, down})
        jointButtons->addWidget(b);
    jointButtons->addStretch(1);
    body->addLayout(jointButtons);
    layout->addWidget(m_body);

    m_status = new QLabel;
    m_status->setWordWrap(true);
    m_status->hide();
    layout->addWidget(m_status);

    auto* bottom = new QHBoxLayout;
    auto* import = new QPushButton(tr("Import..."));
    import->setToolTip(tr("Add joint standards from a file someone shared"));
    auto* exportButton = new QPushButton(tr("Export..."));
    exportButton->setToolTip(tr("Save this joint standard to a file, to share or back up"));
    auto* close = new QDialogButtonBox(QDialogButtonBox::Close);
    bottom->addWidget(import);
    bottom->addWidget(exportButton);
    bottom->addStretch(1);
    bottom->addWidget(close);
    layout->addLayout(bottom);
    resize(620, 520);

    connect(close, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(m_standard, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_loading)
            return;
        standardsfile::setActiveName(i > 0 ? m_standard->currentText() : QString());
        showStandard();
        emit changed();
    });
    connect(m_notes, &QPlainTextEdit::textChanged, this, [this] {
        const int i = current();
        if (m_loading || i < 0)
            return;
        m_list[std::size_t(i)].notes = m_notes->toPlainText().toStdString();
        store();
    });
    connect(add, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("New Joint Standard"), tr("Name (a figure line, a scale...):"),
                                  QLineEdit::Normal,
                                  QString::fromStdString(standards::uniqueName(m_list, "My figure line")), &ok)
                .trimmed();
        if (!ok || name.isEmpty())
            return;
        standards::Standard s;
        s.name = standards::uniqueName(m_list, name.toStdString());
        m_list.push_back(s);
        standardsfile::setActiveName(QString::fromStdString(s.name));
        store();
        reload();
        emit changed();
    });
    connect(rename, &QPushButton::clicked, this, [this] {
        const int i = current();
        if (i < 0)
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename Joint Standard"), tr("Name:"), QLineEdit::Normal,
                                                   QString::fromStdString(m_list[std::size_t(i)].name), &ok)
                                 .trimmed();
        if (!ok || name.isEmpty() || name.toStdString() == m_list[std::size_t(i)].name)
            return;
        auto others = m_list;
        others.erase(others.begin() + i);
        m_list[std::size_t(i)].name = standards::uniqueName(others, name.toStdString());
        standardsfile::setActiveName(QString::fromStdString(m_list[std::size_t(i)].name));
        store();
        reload();
        emit changed();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int i = current();
        if (i < 0)
            return;
        if (QMessageBox::question(this, tr("Delete Joint Standard"),
                                  tr("Delete the joint standard '%1' and its %2 joints? Shapes already made with "
                                     "it stay as they are.")
                                      .arg(QString::fromStdString(m_list[std::size_t(i)].name))
                                      .arg(m_list[std::size_t(i)].joints.size())) != QMessageBox::Yes)
            return;
        m_list.erase(m_list.begin() + i);
        standardsfile::setActiveName(QString());
        store();
        reload();
        emit changed();
    });
    const auto selectedJoint = [this] {
        const auto items = m_joints->selectedItems();
        return items.isEmpty() ? -1 : m_joints->indexOfTopLevelItem(items.front());
    };
    connect(renameJoint, &QPushButton::clicked, this, [this, selectedJoint] {
        const int i = current(), j = selectedJoint();
        if (i < 0 || j < 0)
            return;
        auto& joints = m_list[std::size_t(i)].joints;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename Joint"), tr("Name:"), QLineEdit::Normal,
                                                   QString::fromStdString(joints[std::size_t(j)].name), &ok)
                                 .trimmed();
        if (!ok || name.isEmpty())
            return;
        for (std::size_t k = 0; k < joints.size(); ++k)
            if (int(k) != j && joints[k].name == name.toStdString()) {
                setStatus(tr("There's already a joint called '%1'.").arg(name), true);
                return;
            }
        joints[std::size_t(j)].name = name.toStdString();
        store();
        showStandard();
        emit changed();
    });
    connect(removeJoint, &QPushButton::clicked, this, [this, selectedJoint] {
        const int i = current(), j = selectedJoint();
        if (i < 0 || j < 0)
            return;
        auto& joints = m_list[std::size_t(i)].joints;
        joints.erase(joints.begin() + j);
        store();
        showStandard();
        emit changed();
    });
    const auto move = [this, selectedJoint](int by) {
        const int i = current(), j = selectedJoint();
        if (i < 0 || j < 0)
            return;
        auto& joints = m_list[std::size_t(i)].joints;
        const int to = j + by;
        if (to < 0 || to >= int(joints.size()))
            return;
        std::swap(joints[std::size_t(j)], joints[std::size_t(to)]);
        store();
        showStandard();
        m_joints->setCurrentItem(m_joints->topLevelItem(to));
        emit changed();
    };
    connect(up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(down, &QPushButton::clicked, this, [move] { move(1); });
    connect(import, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Import Joint Standards"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            tr("Joint standards (*.json);;All files (*)"));
        if (file.isEmpty())
            return;
        QFile in(file);
        if (!in.open(QIODevice::ReadOnly)) {
            setStatus(tr("That file can't be opened."), true);
            return;
        }
        try {
            const auto read = standards::fromJson(in.readAll().toStdString());
            QString last;
            for (standards::Standard s : read) {
                s.name = standards::uniqueName(m_list, s.name);
                last = QString::fromStdString(s.name);
                m_list.push_back(std::move(s));
            }
            if (!last.isEmpty())
                standardsfile::setActiveName(last);
            store();
            reload();
            setStatus(read.size() == 1 ? tr("Imported 1 joint standard.")
                                       : tr("Imported %1 joint standards.").arg(read.size()),
                      false);
            emit changed();
        } catch (const std::exception& e) {
            setStatus(QString::fromUtf8(e.what()), true);
        }
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        const int i = current();
        if (i < 0) {
            setStatus(tr("Choose the joint standard to export first."), true);
            return;
        }
        const QString file = QFileDialog::getSaveFileName(
            this, tr("Export Joint Standard"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QLatin1Char('/') +
                QString::fromStdString(m_list[std::size_t(i)].name) + QStringLiteral(".json"),
            tr("Joint standards (*.json)"));
        if (file.isEmpty())
            return;
        QSaveFile out(file);
        const std::string text = standards::toJson({m_list[std::size_t(i)]});
        if (!out.open(QIODevice::WriteOnly) || out.write(text.data(), qint64(text.size())) < 0 || !out.commit()) {
            setStatus(tr("That file can't be written."), true);
            return;
        }
        setStatus(tr("Exported to %1.").arg(QDir::toNativeSeparators(file)), false);
    });
    reload();
}

void JointStandardsDialog::reload()
{
    try {
        m_list = standardsfile::load();
        setStatus({}, false);
    } catch (const std::exception& e) {
        m_list.clear();
        setStatus(QString::fromUtf8(e.what()), true);
    }
    m_loading = true;
    m_standard->clear();
    m_standard->addItem(tr("None"));
    int chosen = 0;
    const QString active = standardsfile::activeName();
    for (std::size_t i = 0; i < m_list.size(); ++i) {
        const QString name = QString::fromStdString(m_list[i].name);
        m_standard->addItem(name);
        if (name == active)
            chosen = int(i) + 1;
    }
    m_standard->setCurrentIndex(chosen);
    m_loading = false;
    showStandard();
}

int JointStandardsDialog::current() const
{
    const int i = m_standard->currentIndex() - 1;
    return i >= 0 && i < int(m_list.size()) ? i : -1;
}

void JointStandardsDialog::showStandard()
{
    const int i = current();
    m_body->setEnabled(i >= 0);
    m_loading = true;
    m_joints->clear();
    m_notes->setPlainText(i >= 0 ? QString::fromStdString(m_list[std::size_t(i)].notes) : QString());
    if (i >= 0)
        for (const auto& j : m_list[std::size_t(i)].joints)
            m_joints->addTopLevelItem(new QTreeWidgetItem(
                {QString::fromStdString(j.name), kindOf(j), QString::fromStdString(standards::describe(j))}));
    m_loading = false;
}

void JointStandardsDialog::store()
{
    try {
        standardsfile::save(m_list);
    } catch (const std::exception& e) {
        setStatus(QString::fromUtf8(e.what()), true);
    }
}

void JointStandardsDialog::setStatus(const QString& text, bool problem)
{
    m_status->setText(text);
    m_status->setStyleSheet(problem ? QStringLiteral("color: #d04020;") : QString());
    m_status->setVisible(!text.isEmpty());
}

}  // namespace forge::app
