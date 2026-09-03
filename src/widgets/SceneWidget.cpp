// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#include "SceneWidget.h"

#include <QQmlContext>
#include <QQuickItem>

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
    this->callScene("suspend");
}

void SceneWidget::showEvent(QShowEvent *event) {
    QQuickWidget::showEvent(event);
    this->callScene("restart");
}

void SceneWidget::hideEvent(QHideEvent *event) {
    this->callScene("suspend");
    QQuickWidget::hideEvent(event);
}

void SceneWidget::resizeEvent(QResizeEvent *event) {
    QQuickWidget::resizeEvent(event);
    if (this->isVisible())
        this->callScene("restart");
}

void SceneWidget::callScene(const char *method) {
    if (auto *root = this->rootObject())
        QMetaObject::invokeMethod(root, method);
}

void SceneWidget::refreshReducedMotion() {
    m_backend->setReducedMotion(conf()->get(Config::reducedMotion).toBool());
}
