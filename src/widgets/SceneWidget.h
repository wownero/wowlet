// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_SCENEWIDGET_H
#define WOWLET_SCENEWIDGET_H

#include <QObject>
#include <QQuickWidget>

// Context object exposed to scene.qml as `sceneCtx`. Carries the reduced-motion
// setting so the scene freezes into its still (reduced-motion) form.
class SceneBackend : public QObject {
Q_OBJECT
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
public:
    explicit SceneBackend(QObject *parent = nullptr) : QObject(parent) {}
    bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool r) { if (m_reducedMotion != r) { m_reducedMotion = r; emit reducedMotionChanged(); } }
signals:
    void reducedMotionChanged();
private:
    bool m_reducedMotion = false;
};

// wowlet: reusable ambient-scene surface. Hosts scene.qml and reads the
// reduced-motion preference from Config. Drop it behind a form, into a banner,
// or over the window as an idle screensaver.
class SceneWidget : public QQuickWidget {
Q_OBJECT
public:
    explicit SceneWidget(QWidget *parent = nullptr);
    void refreshReducedMotion();
private:
    SceneBackend *m_backend;
};

#endif //WOWLET_SCENEWIDGET_H
