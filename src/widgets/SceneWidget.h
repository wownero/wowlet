// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: The Monero Project

#ifndef WOWLET_SCENEWIDGET_H
#define WOWLET_SCENEWIDGET_H

#include <QObject>
#include <QQuickWidget>

// Context object exposed to scene.qml as `sceneCtx`. Carries the reduced-motion
// setting so the scene freezes into its still (reduced-motion) form, and the
// sky variant so the tabs are not all showing the same field.
class SceneBackend : public QObject {
Q_OBJECT
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(int variant READ variant NOTIFY variantChanged)
public:
    explicit SceneBackend(QObject *parent = nullptr) : QObject(parent) {}
    bool reducedMotion() const { return m_reducedMotion; }
    void setReducedMotion(bool r) { if (m_reducedMotion != r) { m_reducedMotion = r; emit reducedMotionChanged(); } }
    int variant() const { return m_variant; }
    void setVariant(int v) { if (m_variant != v) { m_variant = v; emit variantChanged(); } }
signals:
    void reducedMotionChanged();
    void variantChanged();
private:
    bool m_reducedMotion = false;
    int  m_variant = 0;
};

// wowlet: reusable ambient-scene surface. Hosts scene.qml and reads the
// reduced-motion preference from Config. Drop it behind a form, into a banner,
// or over the window as an idle screensaver.
class SceneWidget : public QQuickWidget {
Q_OBJECT
public:
    // Sky variants, one per host, so History/Send/Receive do not all show the
    // same field. Must match the `skies` table in scene.qml.
    enum Variant { Dusk = 0, Dawn = 1, Night = 2 };

    explicit SceneWidget(QWidget *parent = nullptr, Variant variant = Dusk);
    void refreshReducedMotion();
private:
    SceneBackend *m_backend;
};

#endif //WOWLET_SCENEWIDGET_H
