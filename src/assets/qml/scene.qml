import QtQuick

// wowlet ambient scene — dusk sky, tiled grass, and a doge that runs the field
// collecting WOW coins. All motion gates on `scene.reducedMotion` (exposed by
// the host SceneBackend); frozen it is the reduced-motion still.
//
// One cycle is three beats, so the scene breathes instead of running flat out:
//
//   1. empty field for a few seconds  (nothing but sky and grass)
//   2. the coins pop in, one at a time
//   3. the doge and the money go by; the doge collects the coins
//
// Motion is driven by a unitless 0..1 `runProgress` rather than by animating
// straight to `scene.width`: an animation whose `to:` is bound to the width
// restarts every time the host widget resizes, which showed up as a slow first
// lap that snapped back to the start once the real width arrived.
Rectangle {
    id: scene

    property bool reducedMotion: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.reducedMotion : false

    // --- cycle timing -------------------------------------------------------
    readonly property int emptyBeat:  4000   // just background, between laps
    readonly property int revealStep:  320   // per coin, popping in one by one
    readonly property int settleBeat:   500   // coins all out, before the run
    readonly property int runBeat:     8000   // doge + money crossing

    // --- ground plane and the doge's run ------------------------------------
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

    // --- the coins ----------------------------------------------------------
    readonly property int  coinSize: 34
    readonly property real coinRestY: groundY - coinSize + 8
    readonly property int  coinCount: Math.max(3, Math.min(7, Math.floor(scene.width / 190)))
    // Rises 0 -> coinCount during beat 2; coin `index` is out once it passes index+1.
    property real coinReveal: 0.0

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

    // Money crosses with the doge, a little quicker, so it leads him off screen.
    AnimatedImage {
        source: "qrc:/scene/flyingmoney.gif"
        playing: !scene.reducedMotion
        width: 60; height: 55
        y: Math.max(4, Math.min(scene.height * 0.10, 60))
        x: scene.reducedMotion ? scene.width * 0.30
                               : -80 + scene.runProgress * (scene.width + 240)
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

            // Both states are derived from the cycle, so a lap needs no reset
            // bookkeeping: `coinReveal` dropping to 0 hides them, and the doge
            // returning to the left edge un-collects them.
            readonly property bool revealed:  scene.reducedMotion || scene.coinReveal >= index + 1
            readonly property bool collected: revealed && !scene.reducedMotion
                                              && scene.snoutX >= x + width / 2

            AnimatedImage {
                id: coinImage
                width: parent.width; height: parent.height
                source: "qrc:/scene/goldcoin.gif"
                playing: !scene.reducedMotion && coin.revealed && !coin.collected
                opacity: (coin.revealed && !coin.collected) ? 1.0 : 0.0
                scale: !coin.revealed ? 0.2 : (coin.collected ? 1.6 : 1.0)
                // Idle bob, low over the grass.
                SequentialAnimation on y {
                    running: !scene.reducedMotion && coin.revealed && !coin.collected
                    loops: Animation.Infinite
                    NumberAnimation { to: -11; duration: 950 + index * 130; easing.type: Easing.OutQuad }
                    NumberAnimation { to: 0;   duration: 950 + index * 130; easing.type: Easing.InQuad }
                }
                Behavior on opacity { NumberAnimation { duration: 240 } }
                Behavior on scale   { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
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

    // The three beats, on a loop.
    SequentialAnimation {
        running: !scene.reducedMotion
        loops: Animation.Infinite

        // 1. empty field — doge and money are parked off the left edge by
        //    runProgress 0, and every coin is hidden.
        ScriptAction { script: { scene.runProgress = 0.0; scene.coinReveal = 0.0; } }
        PauseAnimation { duration: scene.emptyBeat }

        // 2. coins pop in, one at a time
        NumberAnimation {
            target: scene; property: "coinReveal"
            from: 0.0; to: scene.coinCount
            duration: scene.coinCount * scene.revealStep
        }
        PauseAnimation { duration: scene.settleBeat }

        // 3. the run
        NumberAnimation {
            target: scene; property: "runProgress"
            from: 0.0; to: 1.0
            duration: scene.runBeat
        }
    }
}
