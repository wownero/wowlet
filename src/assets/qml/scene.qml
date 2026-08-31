import QtQuick

// wowlet ambient scene — a doge running a field of WOW coins under a big sky.
// All motion gates on `scene.reducedMotion` (exposed by the host SceneBackend);
// frozen it is the reduced-motion still.
//
// One cycle is three beats, so the scene breathes instead of running flat out:
//
//   1. an empty field for 3-5s (nothing but sky and grass)
//   2. the coins pop in, one at a time
//   3. the doge and the money go by, and the doge collects the coins
//
// Every lap re-rolls its own timing — how long the field stays empty, when the
// money enters relative to the doge and how fast it crosses — so the loop does
// not read as the same eight seconds forever. Roughly one lap in six, the doge
// eats it halfway down the field and the lap ends in GAME OVER with the rest of
// the coins uncollected.
//
// Motion is driven by a unitless 0..1 `runProgress` rather than by animating
// straight to `scene.width`: an animation whose `to:` is bound to the width
// restarts every time the host widget resizes, which showed up as a slow first
// lap that snapped back to the start once the real width arrived.
Rectangle {
    id: scene

    property bool reducedMotion: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.reducedMotion : false
    // 0 dusk · 1 dawn · 2 night — so the tabs are not all the same field.
    property int  variant:       (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.variant : 0

    // --- sky ----------------------------------------------------------------
    readonly property var skies: [
        ["#160d2b", "#5b2f6e", "#c96aa0", "#f2adc9"],   // dusk
        ["#123a63", "#2f6fa8", "#e8a06a", "#ffd9a8"],   // dawn
        ["#04060f", "#0d1330", "#1d2450", "#39406e"]    // night
    ]
    readonly property var sky: skies[Math.max(0, Math.min(skies.length - 1, variant))]
    readonly property bool night: variant === 2

    // --- cycle timing (re-rolled every lap) ---------------------------------
    // emptyBeat / moneyDelay / moneyBeat are rolled onto the animations directly
    // (see the ScriptAction at the foot of the file).
    readonly property int revealStep:  320   // per coin, popping in one by one
    readonly property int settleBeat:  500   // coins all out, before the run
    readonly property int runBeat:     8000  // the doge crossing

    // --- the trip -----------------------------------------------------------
    property bool tripLap: false     // does the doge eat it this lap?
    property real tripAt:  0.55      // ...and how far along
    readonly property bool tripped: tripLap && runProgress >= tripAt

    // --- ground plane and the doge's run ------------------------------------
    readonly property int  grassHeight: 46
    readonly property int  dogWidth:    104
    readonly property int  dogHeight:   83
    readonly property real groundY:     scene.height - grassHeight
    readonly property real runStart:    -dogWidth - 10
    readonly property real runEnd:      scene.width + 10

    property real runProgress: 0.0
    property real moneyProgress: 0.0
    // A tripped doge stops where he fell instead of sliding on to the far edge.
    readonly property real dogProgress: tripped ? tripAt : runProgress
    readonly property real dogX: runStart + dogProgress * (runEnd - runStart)
    // The doge collects a coin once its snout reaches the coin's centre.
    readonly property real snoutX: dogX + dogWidth * 0.78

    // --- the coins ----------------------------------------------------------
    readonly property int  coinSize: 34
    readonly property real coinRestY: groundY - coinSize + 8
    readonly property int  coinCount: Math.max(3, Math.min(7, Math.floor(scene.width / 190)))
    // Rises 0 -> coinCount during beat 2; coin `index` is out once it passes index+1.
    property real coinReveal: 0.0

    gradient: Gradient {
        GradientStop { position: 0.0;  color: scene.sky[0] }
        GradientStop { position: 0.5;  color: scene.sky[1] }
        GradientStop { position: 0.82; color: scene.sky[2] }
        GradientStop { position: 1.0;  color: scene.sky[3] }
    }

    // Stars, night only. Positions are a fixed hash of the index so they do not
    // jump around on every repaint.
    Repeater {
        model: scene.night ? 26 : 0
        Rectangle {
            width: (index % 4 === 0) ? 3 : 2
            height: width
            radius: 1
            color: "#fdf6d8"
            opacity: 0.45 + ((index * 37) % 50) / 100
            x: scene.width  * (((index * 73) % 100) / 100)
            y: scene.height * 0.06 + scene.height * 0.42 * (((index * 41) % 100) / 100)
        }
    }

    // Sun, or a paler moon at night.
    Rectangle {
        x: scene.width * 0.74
        y: scene.height * (scene.night ? 0.10 : 0.14)
        width: scene.night ? 46 : 62
        height: width
        radius: width / 2
        color: scene.night ? "#e8ecff" : "#ffe9b0"
        opacity: scene.night ? 0.85 : 0.9
    }

    // Money crosses on its own roll, so it does not shadow the doge every lap.
    AnimatedImage {
        source: "qrc:/scene/flyingmoney.gif"
        playing: !scene.reducedMotion
        width: 60; height: 55
        y: Math.max(4, Math.min(scene.height * 0.10, 60))
        x: scene.reducedMotion ? scene.width * 0.30
                               : -80 + scene.moneyProgress * (scene.width + 240)
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
            // returning to the left edge un-collects them. A tripped doge never
            // reaches the rest, so they are still sitting there at GAME OVER.
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
        id: doge
        source: "qrc:/scene/dog.gif"
        playing: !scene.reducedMotion && !scene.tripped
        width: scene.dogWidth; height: scene.dogHeight
        y: scene.groundY - scene.dogHeight + 21
        x: scene.reducedMotion ? 8 : scene.dogX
        // Face-plant: tips forward over his own feet rather than sinking
        // through the grass, so he ends up nose-down on the ground.
        transformOrigin: Item.Bottom
        rotation: scene.tripped ? 58 : 0
        Behavior on rotation { NumberAnimation { duration: 260; easing.type: Easing.OutQuad } }
    }

    Text {
        anchors.centerIn: parent
        text: "GAME OVER"
        color: "#ff6b6b"
        style: Text.Outline
        styleColor: "#1a0b1f"
        font.bold: true
        font.pixelSize: Math.max(20, Math.min(scene.height * 0.20, 34))
        opacity: scene.tripped ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 320 } }
    }

    // The three beats, on a loop, re-rolled each time round.
    //
    // This is a timer-driven state machine rather than one big
    // SequentialAnimation on purpose: each lap rolls its own durations, and
    // assigning a duration to a child of a *running* SequentialAnimation makes
    // the group recompute and re-enter its own ScriptAction, which recurses
    // until the JS stack blows.
    property int beat: 0

    NumberAnimation {
        id: revealAnim
        target: scene; property: "coinReveal"
        from: 0.0; to: scene.coinCount
        duration: scene.coinCount * scene.revealStep
    }
    NumberAnimation {
        id: runAnim
        target: scene; property: "runProgress"
        from: 0.0; to: 1.0
        duration: scene.runBeat
    }
    NumberAnimation {
        id: moneyAnim
        target: scene; property: "moneyProgress"
        from: 0.0; to: 1.0
        duration: 6000
    }

    Timer { id: beatTimer;  repeat: false; onTriggered: scene.advance() }
    Timer { id: moneyTimer; repeat: false; onTriggered: moneyAnim.start() }

    // Beat 0 is the empty field, 1 is the coins coming out, 2 is the run.
    function startLap() {
        beatTimer.stop(); moneyTimer.stop();
        revealAnim.stop(); runAnim.stop(); moneyAnim.stop();

        scene.beat = 0;
        scene.runProgress = 0.0;
        scene.moneyProgress = 0.0;
        scene.coinReveal = 0.0;

        moneyAnim.duration  = 5000 + Math.floor(Math.random() * 2000);
        moneyTimer.interval = Math.floor(Math.random() * Math.max(1, scene.runBeat - moneyAnim.duration));
        scene.tripLap = Math.random() < 0.17;
        scene.tripAt  = 0.34 + Math.random() * 0.34;

        beatTimer.interval = 3000 + Math.floor(Math.random() * 2000);   // 3-5s of empty field
        beatTimer.start();
    }

    function advance() {
        if (scene.reducedMotion)
            return;
        if (scene.beat === 0) {
            scene.beat = 1;
            revealAnim.start();
            beatTimer.interval = scene.coinCount * scene.revealStep + scene.settleBeat;
            beatTimer.start();
        } else if (scene.beat === 1) {
            scene.beat = 2;
            runAnim.start();
            moneyTimer.start();
            beatTimer.interval = scene.runBeat + 900;   // 900 holds on GAME OVER
            beatTimer.start();
        } else {
            scene.startLap();
        }
    }

    Component.onCompleted: if (!scene.reducedMotion) scene.startLap();

    onReducedMotionChanged: {
        if (scene.reducedMotion) {
            beatTimer.stop(); moneyTimer.stop();
            revealAnim.stop(); runAnim.stop(); moneyAnim.stop();
            scene.tripLap = false;
        } else {
            scene.startLap();
        }
    }
}
