// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "SceneWidget.h"

#include <QQmlContext>

#include "utils/config.h"

SceneWidget::SceneWidget(QWidget *parent, Variant variant)
    : QQuickWidget(parent)
    , m_backend(new SceneBackend(this))
{
    m_backend->setReducedMotion(conf()->get(Config::reducedMotion).toBool());
    m_backend->setVariant(static_cast<int>(variant));
    this->rootContext()->setContextProperty("sceneCtx", m_backend);
    this->setResizeMode(QQuickWidget::SizeRootObjectToView);
    this->setSource(QUrl("qrc:/qml/scene.qml"));
}

void SceneWidget::refreshReducedMotion() {
    m_backend->setReducedMotion(conf()->get(Config::reducedMotion).toBool());
}
