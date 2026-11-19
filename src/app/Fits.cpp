// SPDX-License-Identifier: GPL-3.0-or-later
#include "Fits.h"

#include <QSettings>
#include <QString>

namespace forge::app::fits {
namespace {

QSettings preferences()
{
    return QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Forge3D"), QStringLiteral("Forge3D"));
}

QString key(bool resin, tolerance::Kind kind)
{
    const char* k = kind == tolerance::Kind::Peg ? "peg" : kind == tolerance::Kind::Ball ? "ball" : "pin";
    return QStringLiteral("fit/%1/%2").arg(resin ? QStringLiteral("resin") : QStringLiteral("fdm"),
                                          QString::fromLatin1(k));
}

}  // namespace

std::optional<double> measured(bool resin, tolerance::Kind kind)
{
    const QVariant v = preferences().value(key(resin, kind));
    bool ok = false;
    const double fit = v.toDouble(&ok);
    if (!v.isValid() || !ok || fit < 0.0 || fit > 2.0)
        return std::nullopt;
    return fit;
}

void setMeasured(bool resin, tolerance::Kind kind, std::optional<double> fit)
{
    auto p = preferences();
    if (fit)
        p.setValue(key(resin, kind), *fit);
    else
        p.remove(key(resin, kind));
}

double starting(bool resin, tolerance::Kind kind)
{
    return measured(resin, kind).value_or(resin ? 0.1 : 0.2);
}

}  // namespace forge::app::fits
