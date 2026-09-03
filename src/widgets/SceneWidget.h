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

protected:
    // A scene on a hidden tab is parked rather than left running: an unshown
    // QQuickWidget is 0px wide, and a lap run against a 0px field collects every
    // coin at once and leaves its litter (+1s, barks) to surface on the real lap
    // when the tab is finally shown. Parking also keeps a background tab from
    // burning CPU on animation nobody is looking at.
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    // Belt and braces: a scene that is on screen and has just been given a real
    // size is running, whether or not a show event was the thing that got it
    // there. `restart` only sets a flag, so re-arming an already-running scene
    // costs nothing and does not interrupt its lap.
    void resizeEvent(QResizeEvent *event) override;

private:
    void callScene(const char *method);

    SceneBackend *m_backend;
};

#endif //WOWLET_SCENEWIDGET_H
