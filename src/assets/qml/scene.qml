import QtQuick

// wowlet ambient scene — dusk sky, tiled grass, and a doge that runs the length
// of the field collecting WOW coins, then loops. All motion gates on
// `scene.reducedMotion` (exposed by the host SceneBackend); frozen it is the
// reduced-motion still.
//
// Motion is driven by a unitless 0..1 `runProgress` rather than by animating
// straight to `scene.width`: an animation whose `to:` is bound to the width
// restarts every time the host widget resizes, which showed up as a slow first
// lap that snapped back to the start once the real width arrived.
Rectangle {
    id: scene

    property bool reducedMotion: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.reducedMotion : false

    // Ground plane and the doge's run.
    readonly property int  grassHeight: 46
    readonly property int  dogWidth:    104
    readonly property int  dogHeight:   83
    readonly property real groundY:     scene.height - grassHeight
    readonly property real runStart:    -dogWidth - 10
    readonly property real runEnd:      scene.width + 10

    property real runProgress: 0.0
    readonly property real dogX: runStart + runProgress * (runEnd - runStart)
    // The doge collects a coin once its snout reaches the coin's centre.
    readonly property real snoutX: dogX + dogWidth * 0.78

    // Coins rest on the grass so the run reads as a pickup, not a fly-by.
    readonly property int  coinSize: 34
    readonly property real coinRestY: groundY - coinSize + 8
    readonly property int  coinCount: Math.max(3, Math.min(7, Math.floor(scene.width / 190)))

    gradient: Gradient {
        GradientStop { position: 0.0;  color: "#160d2b" }
        GradientStop { position: 0.5;  color: "#5b2f6e" }
        GradientStop { position: 0.82; color: "#c96aa0" }
        GradientStop { position: 1.0;  color: "#f2adc9" }
    }

    // Sun, low in the dusk sky.
    Rectangle {
        x: scene.width * 0.74; y: scene.height * 0.14
        width: 62; height: 62; radius: 31
        color: "#ffe9b0"; opacity: 0.9
    }

    // Money drifting across the sky, on its own unitless lap.
    property real moneyProgress: 0.0
    AnimatedImage {
        source: "qrc:/scene/flyingmoney.gif"
        playing: !scene.reducedMotion
        width: 60; height: 55
        y: 4
        x: scene.reducedMotion ? scene.width * 0.30 : -80 + scene.moneyProgress * (scene.width + 160)
        opacity: 0.9
    }

    Row {
        anchors.bottom: parent.bottom
        Repeater {
            model: Math.ceil(scene.width / 180) + 1
            Image { source: "qrc:/scene/grass.png"; width: 180; height: scene.grassHeight; smooth: false }
        }
    }

    Repeater {
        model: scene.coinCount

        Item {
            id: coin
            width: scene.coinSize; height: scene.coinSize
            // Spread the coins over the field, clear of both ends of the run.
            x: scene.width * (0.22 + 0.66 * (scene.coinCount > 1 ? index / (scene.coinCount - 1) : 0.5)) - width / 2
            y: scene.coinRestY

            // Purely derived from the run, so a lap reset restores every coin
            // with no bookkeeping.
            readonly property bool collected: !scene.reducedMotion && scene.snoutX >= x + width / 2

            AnimatedImage {
                id: coinImage
                width: parent.width; height: parent.height
                source: "qrc:/scene/goldcoin.gif"
                playing: !scene.reducedMotion && !coin.collected
                opacity: coin.collected ? 0.0 : 1.0
                scale: coin.collected ? 1.6 : 1.0
                // Idle bob, low over the grass.
                SequentialAnimation on y {
                    running: !scene.reducedMotion && !coin.collected
                    loops: Animation.Infinite
                    NumberAnimation { to: -11; duration: 950 + index * 130; easing.type: Easing.OutQuad }
                    NumberAnimation { to: 0;   duration: 950 + index * 130; easing.type: Easing.InQuad }
                }
                Behavior on opacity { NumberAnimation { duration: 260 } }
                Behavior on scale   { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
            }

            // Pickup pop.
            Text {
                id: popText
                anchors.horizontalCenter: parent.horizontalCenter
                text: "+1"
                color: "#ffd257"
                style: Text.Outline
                styleColor: "#2a1636"
                font.bold: true
                font.pixelSize: 18
                opacity: 0.0
                y: coin.collected ? -44 : -6
                Behavior on y { NumberAnimation { duration: 900; easing.type: Easing.OutQuad } }
                SequentialAnimation on opacity {
                    running: coin.collected
                    NumberAnimation { to: 1.0; duration: 90 }
                    PauseAnimation  { duration: 420 }
                    NumberAnimation { to: 0.0; duration: 340 }
                }
            }
        }
    }

    AnimatedImage {
        source: "qrc:/scene/dog.gif"
        playing: !scene.reducedMotion
        width: scene.dogWidth; height: scene.dogHeight
        y: scene.groundY - scene.dogHeight + 21
        x: scene.reducedMotion ? 8 : scene.dogX
    }

    // One lap: run the field, then start over with the coins restored.
    NumberAnimation on runProgress {
        running: !scene.reducedMotion
        from: 0.0; to: 1.0
        duration: 8000
        loops: Animation.Infinite
    }

    NumberAnimation on moneyProgress {
        running: !scene.reducedMotion
        from: 0.0; to: 1.0
        duration: 13000
        loops: Animation.Infinite
    }
}
