import QtQuick

// wowlet ambient scene — a doge running a field of WOW coins under a big sky.
// All motion gates on `scene.live` (reduced motion off, the widget laid out and
// on screen); frozen it is the reduced-motion still.
//
// One cycle is three beats, so the scene breathes instead of running flat out:
//
//   1. an empty field for 3-5s (sky, grass, and sometimes a doge sitting in it)
//   2. the coins pop in, one at a time
//   3. the doge and the money go by, and the doge collects the coins
//
// Every lap re-rolls its own timing and its own cast — how long the field stays
// empty, how fast the doge crosses, how many bills blow through (0-3), whether
// one coin is a fat +5, which bark he uses — so the loop does not read as the
// same eight seconds forever. Roughly one lap in six he eats it halfway down the
// field: he face-plants, deforms, throws up dust, barks something undignified,
// and the lap ends in GAME OVER with the rest of the coins uncollected. Clear a
// whole field instead and he hops and gloats.
//
// Motion is driven by a unitless 0..1 `runProgress` rather than by animating
// straight to `scene.width`: an animation whose `to:` is bound to the width
// restarts every time the host widget resizes, which showed up as a slow first
// lap that snapped back to the start once the real width arrived.
//
// Two rules keep the lap honest, both learned from bugs:
//
//   * Nothing runs until the scene is actually laid out and visible. A hidden
//     tab is 0px wide, and at 0px every coin sits at x=0, so the doge "collects"
//     the whole field at once; the leftovers of that phantom lap then showed up
//     on the real one as +1s raining out of an empty sky.
//   * Every one-shot flourish (+1 pop, bark, dust) is started and *reset* from
//     an explicit handler. A `SequentialAnimation on <prop> { running: <bound> }`
//     that gets interrupted leaves the property wherever it stopped, which is
//     how a +1 could hang in the air for the rest of the lap.
Rectangle {
    id: scene

    property bool reducedMotion: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.reducedMotion : false
    // 0 dusk · 1 dawn · 2 night — so the tabs are not all the same field.
    property int  variant:       (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.variant : 0
    // What this surface is doing. History gets the whole run; Receive gets a
    // doge sitting in the field; Send gets money blowing past and nothing else.
    // One scene, three amounts of it, so a form does not have to compete with a
    // dog for attention.
    property string mode: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.mode : "run"
    readonly property bool runMode:   mode === "run"
    readonly property bool sitMode:   mode === "sit"
    readonly property bool moneyMode: mode === "money"

    // The host sets this false while its tab is hidden (see SceneWidget).
    property bool onScreen: true
    // Below this the widget has not been laid out yet and the field is nonsense.
    readonly property bool laidOut: width > 60 && height > 40
    readonly property bool live:    !reducedMotion && laidOut && onScreen

    // --- sky ----------------------------------------------------------------
    readonly property var skies: [
        ["#160d2b", "#5b2f6e", "#c96aa0", "#f2adc9"],   // dusk
        ["#123a63", "#2f6fa8", "#e8a06a", "#ffd9a8"],   // dawn
        ["#04060f", "#0d1330", "#1d2450", "#39406e"]    // night
    ]
    readonly property var sky: skies[Math.max(0, Math.min(skies.length - 1, variant))]
    readonly property bool night: variant === 2

    // --- cycle timing -------------------------------------------------------
    readonly property int revealStep:  320   // per coin, popping in one by one
    readonly property int settleBeat:  500   // coins all out, before the run
    property int runBeat: 8000               // the doge crossing, re-rolled per lap

    // --- the lap ------------------------------------------------------------
    // Bumped by startLap(). Anything holding one-shot state resets on the change
    // rather than trying to work out for itself that a new lap has begun.
    property int  lapId:   0
    property bool tripLap: false     // does the doge eat it this lap?
    property real tripAt:  0.55      // ...and how far along
    readonly property bool tripped: tripLap && runProgress >= tripAt
    property bool idleLap: false     // doge sitting in the field during beat 0
    property bool birdLap: false
    property bool starLap: false     // shooting star, night only
    property int  bonusIndex: -1     // which coin is worth +5, or -1

    property string barkText: ""
    readonly property var crashBarks: ["HRNMMROOF", "BONK", "OOF", "SUCH OW",
                                       "MUCH FALL", "AAARF", "SNOOT.EXE", "WOW... OW",
                                       "HECKIN OW", "BORK BONK"]
    readonly property var winBarks:   ["MUCH COIN", "SUCH SPEED", "VERY WIN", "WOW",
                                       "SO RICH", "AMAZE", "MANY WOW", "GIB MORE"]

    // --- ground plane and the doge's run ------------------------------------
    readonly property int  grassHeight: 46
    readonly property int  dogWidth:    104
    readonly property int  dogHeight:   83
    readonly property real groundY:     scene.height - grassHeight
    readonly property real runStart:    -dogWidth - 10
    readonly property real runEnd:      scene.width + 10

    property real runProgress: 0.0
    // A tripped doge stops where he fell instead of sliding on to the far edge.
    readonly property real dogProgress: tripped ? tripAt : runProgress
    readonly property real dogX: runStart + dogProgress * (runEnd - runStart)
                                + scene.crashSlide
    // The doge collects a coin once his snout reaches the coin's centre.
    readonly property real snoutX: dogX + dogWidth * 0.78
    property real hopY: 0.0          // victory hop

    // Crash rig. Driven by crashAnim, reset by resetCrash().
    //
    // The wipeout is a face-plant and a skid, not a stunt: he pitches nose-down
    // by a small angle, drops onto his snout, and slides forward along the
    // ground while the dust goes up. An earlier version rotated him 66 degrees
    // about a pivot inside the sprite and sheared him at the same time, which
    // launched him into the air at a diagonal and stretched him on the way.
    property real crashRot:    0.0
    property real crashSquashX: 1.0
    property real crashSquashY: 1.0
    property real crashDrop:   0.0   // px down, so the face reaches the ground
    property real crashSlide:  0.0   // px forward along the ground
    property real dustT:       1.0   // 0..1 sweep; 1 is spent

    // --- the coins ----------------------------------------------------------
    readonly property int  coinSize: 34
    readonly property real coinRestY: groundY - coinSize + 8
    readonly property int  coinCount: Math.max(3, Math.min(7, Math.floor(scene.width / 190)))
    // Rises 0 -> coinCount during beat 2; coin `index` is out once it passes index+1.
    property real coinReveal: 0.0
    property int  picked: 0          // coins taken this lap

    // --- the bills ----------------------------------------------------------
    // One roll per lap: how many blow through, when, how fast, how high, how big.
    property var moneyRolls: []

    function rerollMoney(index) {
        var rolls = scene.moneyRolls.slice();
        var fresh = scene.rollMoney();
        rolls[index] = fresh[Math.floor(Math.random() * fresh.length)];
        // Re-roll whether this bill flies at all, so the sky thins and fills
        // instead of carrying the same traffic for ever.
        rolls[index].active = Math.random() < 0.72;
        scene.moneyRolls = rolls;
    }

    function rollMoney() {
        var n = [0, 1, 1, 2, 2, 3][Math.floor(Math.random() * 6)];
        var out = [];
        for (var i = 0; i < 3; ++i) {
            out.push({
                "active": i < n,
                "delay":  Math.floor(Math.random() * (scene.moneyMode ? 7000
                          : Math.max(1, scene.runBeat - 2500))),
                "dur":    4200 + Math.floor(Math.random() * 3600),
                "band":   Math.random(),
                "size":   0.72 + Math.random() * 0.55,
                "bob":    6 + Math.random() * 12,
                "tumble": Math.random() * 2.5
            });
        }
        return out;
    }

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
            SequentialAnimation on opacity {
                running: scene.live && index % 3 === 0
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 1400 + (index * 211) % 1800 }
                NumberAnimation { to: 0.95; duration: 1400 + (index * 173) % 1600 }
            }
        }
    }

    // A shooting star, rolled per lap, night only.
    Rectangle {
        id: shootingStar
        property real p: 1.0
        readonly property real originX: scene.width * 0.62
        readonly property real originY: scene.height * 0.10
        width: 34; height: 2; radius: 1; rotation: 28
        color: "#fff6d0"
        visible: scene.night && scene.starLap && p < 1.0
        opacity: Math.sin(Math.PI * Math.min(1, Math.max(0, p))) * 0.9
        x: originX - 120 + p * (scene.width * 0.5)
        y: originY + p * (scene.height * 0.28)
        NumberAnimation { id: starAnim; target: shootingStar; property: "p"; from: 0; to: 1; duration: 1100 }
        Timer {
            id: starTimer; repeat: false
            onTriggered: if (scene.live && scene.starLap) starAnim.restart()
        }
    }

    // Sun, or a paler moon at night, with a soft corona.
    Item {
        id: sunDisc
        x: scene.width * 0.74
        y: scene.height * (scene.night ? 0.10 : 0.14)
        width: scene.night ? 46 : 62
        height: width
        // Stacked rings rather than one disc: a soft falloff instead of a hard
        // circle drawn around the sun.
        Repeater {
            model: 6
            Rectangle {
                anchors.centerIn: parent
                width: sunDisc.width * (2.4 - index * 0.26); height: width; radius: width / 2
                color: scene.night ? "#8fa0ff" : "#ffd07a"
                opacity: 0.028 + index * 0.012
            }
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: scene.night ? "#e8ecff" : "#ffe9b0"
            opacity: scene.night ? 0.85 : 0.9
        }
    }

    // Clouds, drifting on their own clock. Each is three blobs so it reads as a
    // cloud and not a pill.
    Repeater {
        model: scene.night ? 2 : 3
        Item {
            id: cloud
            readonly property real seed: ((index * 61) % 100) / 100
            property real p: seed
            readonly property real w: 54 + seed * 46
            width: w; height: w * 0.42
            x: -w - 20 + p * (scene.width + w * 2 + 40)
            y: scene.height * (0.08 + 0.30 * (((index * 43) % 100) / 100))
            opacity: scene.night ? 0.16 : 0.30
            Rectangle { x: 0;                 y: parent.height * 0.36; width: parent.width * 0.52; height: parent.height * 0.62; radius: height / 2; color: "#ffffff" }
            Rectangle { x: parent.width*0.24; y: 0;                    width: parent.width * 0.50; height: parent.height;        radius: height / 2; color: "#ffffff" }
            Rectangle { x: parent.width*0.50; y: parent.height * 0.30; width: parent.width * 0.50; height: parent.height * 0.68; radius: height / 2; color: "#ffffff" }
            NumberAnimation on p {
                running: scene.live
                loops: Animation.Infinite
                from: cloud.seed; to: 1.0
                duration: (1.0 - cloud.seed) * (52000 + index * 14000)
            }
        }
    }

    // Two birds, rolled per lap, daytime skies only.
    Repeater {
        model: (scene.night || !(scene.birdLap || scene.sitMode)) ? 0 : 2
        Item {
            id: bird
            property real p: 0
            readonly property real lift: 5 + index * 3
            width: 13; height: 7
            x: -20 + p * (scene.width + 60)
            y: scene.height * (0.16 + 0.12 * index) + Math.sin(p * 9 + index) * lift
            opacity: 0.75
            Rectangle {
                x: 0; y: 3; width: 8; height: 1.6; radius: 1; color: "#2b1a3a"
                transformOrigin: Item.Right
                rotation: -22 + Math.sin(bird.p * 46 + index) * 16
            }
            Rectangle {
                x: 6; y: 3; width: 8; height: 1.6; radius: 1; color: "#2b1a3a"
                transformOrigin: Item.Left
                rotation: 22 - Math.sin(bird.p * 46 + index) * 16
            }
            NumberAnimation on p {
                running: scene.live && (scene.birdLap || scene.sitMode)
                loops: Animation.Infinite
                from: 0; to: 1
                duration: 21000 + index * 7000
            }
        }
    }

    // The bills. Zero to three per lap, each on its own delay, speed and band,
    // so the sky is not metronomic.
    Repeater {
        model: 3
        AnimatedImage {
            id: bill
            readonly property var roll: (scene.moneyRolls.length > index) ? scene.moneyRolls[index] : null
            property real p: 0
            source: "qrc:/scene/flyingmoney.gif"
            playing: scene.live && bill.visible
            visible: scene.sitMode ? false
                     : scene.reducedMotion ? index === 0
                     : (roll !== null && roll.active && p > 0 && p < 1)
            width: 60 * (roll ? roll.size : 1); height: 55 * (roll ? roll.size : 1)
            x: scene.reducedMotion ? scene.width * 0.30
                                   : -width - 20 + p * (scene.width + width * 2 + 40)
            y: {
                var band = roll ? roll.band : 0.4;
                var top  = Math.max(2, scene.height * 0.04);
                var span = Math.max(10, scene.height * 0.42 - height * 0.4);
                return top + band * span + (roll ? Math.sin(p * 7.5) * roll.bob : 0);
            }
            rotation: roll ? Math.sin(p * (5 + roll.tumble) + index) * (6 + roll.tumble * 4) : 0
            opacity: 0.92

            NumberAnimation {
                id: flyAnim
                target: bill; property: "p"; from: 0; to: 1
                duration: bill.roll ? bill.roll.dur : 6000
                onFinished: bill.rescheduleSelf()
            }
            Timer {
                id: flyTimer; repeat: false
                interval: bill.roll ? bill.roll.delay : 0
                onTriggered: {
                    if (bill.roll && bill.roll.active)
                        flyAnim.restart();
                    else
                        bill.rescheduleSelf();   // dormant this pass; try again later
                }
            }

            function armFor(beat) {
                flyAnim.stop();
                bill.p = 0;
                flyTimer.stop();
                // In money mode every lap is the money beat.
                if ((beat === 2 || scene.moneyMode) && scene.live
                        && bill.roll && bill.roll.active)
                    flyTimer.restart();
            }

            // Money mode has no lap to hide inside, so each bill runs its own
            // clock: cross, re-roll, wait, cross again. A shared cycle was
            // cutting bills off mid-screen whenever a roll's delay plus its
            // duration ran past the cycle length, which looked exactly like
            // money vanishing halfway across.
            function rescheduleSelf() {
                if (!scene.moneyMode || !scene.live)
                    return;
                scene.rerollMoney(index);
                bill.p = 0;
                flyTimer.interval = 900 + Math.floor(Math.random() * 5200);
                flyTimer.restart();
            }
            Connections {
                target: scene
                function onBeatChanged() { bill.armFor(scene.beat) }
                function onLapIdChanged() { bill.armFor(-1) }
            }
        }
    }

    Row {
        anchors.bottom: parent.bottom
        Repeater {
            model: Math.ceil(scene.width / 180) + 1
            Image { source: "qrc:/scene/grass.png"; width: 180; height: scene.grassHeight; smooth: false }
        }
    }

    Repeater {
        model: scene.runMode ? scene.coinCount : 0

        Item {
            id: coin
            readonly property bool bonus: scene.bonusIndex === index
            readonly property int  worth: bonus ? 5 : 1
            width: scene.coinSize * (bonus ? 1.34 : 1.0)
            height: width
            // Spread the coins over the field, clear of both ends of the run.
            // The last coin sits clear of the right edge so the gloat that
            // follows it plays out on screen instead of half off it.
            x: scene.width * (0.20 + 0.60 * (scene.coinCount > 1 ? index / (scene.coinCount - 1) : 0.5)) - width / 2
            y: scene.coinRestY - (bonus ? 6 : 0)

            // Both states are derived from the cycle, so a lap needs no reset
            // bookkeeping: `coinReveal` dropping to 0 hides them, and the doge
            // returning to the left edge un-collects them. A tripped doge never
            // reaches the rest, so they are still sitting there at GAME OVER.
            readonly property bool revealed:  scene.reducedMotion || scene.coinReveal >= index + 1
            readonly property bool collected: revealed && scene.live && scene.beat === 2
                                              && scene.snoutX >= x + width / 2

            // One-shot, so it is started and cleared by hand — a bound `running:`
            // leaves the +1 stranded wherever an interrupted lap dropped it.
            onCollectedChanged: {
                popAnim.stop();
                plus.opacity = 0.0;
                plus.y = -6;
                if (coin.collected) {
                    popAnim.start();
                    scene.pick(coin.worth);
                }
            }

            // The fat coin gets a slow halo so it reads as worth chasing.
            Rectangle {
                anchors.centerIn: coinImage
                width: coin.width * 1.5; height: width; radius: width / 2
                color: "#ffd257"
                visible: coin.bonus && coin.revealed && !coin.collected
                opacity: 0.18
                SequentialAnimation on scale {
                    running: coin.bonus && scene.live && coin.revealed && !coin.collected
                    loops: Animation.Infinite
                    NumberAnimation { to: 1.18; duration: 780; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 0.92; duration: 780; easing.type: Easing.InOutQuad }
                }
            }

            AnimatedImage {
                id: coinImage
                width: parent.width; height: parent.height
                source: "qrc:/scene/goldcoin.gif"
                playing: scene.live && coin.revealed && !coin.collected
                opacity: (coin.revealed && !coin.collected) ? 1.0 : 0.0
                scale: !coin.revealed ? 0.2 : (coin.collected ? 1.6 : 1.0)
                // Idle bob, low over the grass.
                SequentialAnimation on y {
                    running: scene.live && coin.revealed && !coin.collected
                    loops: Animation.Infinite
                    NumberAnimation { to: -11; duration: 950 + index * 130; easing.type: Easing.OutQuad }
                    NumberAnimation { to: 0;   duration: 950 + index * 130; easing.type: Easing.InQuad }
                }
                Behavior on opacity { NumberAnimation { duration: 240 } }
                Behavior on scale   { NumberAnimation { duration: 300; easing.type: Easing.OutBack } }
            }

            // Pickup pop.
            Text {
                id: plus
                anchors.horizontalCenter: parent.horizontalCenter
                text: "+" + coin.worth
                color: coin.bonus ? "#ffe9a3" : "#ffd257"
                style: Text.Outline
                styleColor: "#2a1636"
                font.bold: true
                font.pixelSize: coin.bonus ? 24 : 18
                opacity: 0.0
                y: -6
            }
            SequentialAnimation {
                id: popAnim
                ParallelAnimation {
                    NumberAnimation { target: plus; property: "opacity"; to: 1.0; duration: 90 }
                    NumberAnimation { target: plus; property: "y"; from: -6; to: -46; duration: 820; easing.type: Easing.OutQuad }
                }
                PauseAnimation  { duration: 260 }
                NumberAnimation { target: plus; property: "opacity"; to: 0.0; duration: 320 }
            }

            // Sparkle burst on pickup.
            Repeater {
                model: 5
                Rectangle {
                    readonly property real ang: (index / 5) * Math.PI * 2 + index
                    width: 3; height: 3; radius: 1.5
                    color: "#ffe9a3"
                    visible: scene.sparkT < 1 && coin.collected
                    opacity: (1.0 - scene.sparkT) * 0.9
                    x: coin.width / 2 - 1.5 + Math.cos(ang) * scene.sparkT * 26
                    y: coin.height / 2 - 1.5 + Math.sin(ang) * scene.sparkT * 26 - scene.sparkT * 8
                }
            }
        }
    }

    // Sparkle sweep, shared by whichever coin was last taken.
    property real sparkT: 1.0
    NumberAnimation { id: sparkAnim; target: scene; property: "sparkT"; from: 0; to: 1; duration: 520 }

    // A coin sails past on the Send screen — money going somewhere.
    AnimatedImage {
        id: flyingCoin
        property real t: 1.0
        property real band: 0.3
        source: "qrc:/scene/goldcoin.gif"
        playing: scene.live && visible
        visible: !scene.reducedMotion && scene.moneyMode && t < 1.0
        width: 26; height: 26
        x: -width + t * (scene.width + width * 2)
        // A shallow arc rather than a flat line, so it reads as thrown.
        y: scene.height * band - Math.sin(t * Math.PI) * scene.height * 0.16
        rotation: t * 540
        opacity: Math.min(1, Math.sin(t * Math.PI) * 3)
        NumberAnimation { id: flyingCoinAnim; target: flyingCoin; property: "t"
                          from: 0; to: 1; duration: 2600 }
    }

    // ...and one lands in the field on the Receive screen. A coin arriving is
    // the whole point of that tab, so it is worth more than a doge panting.
    Item {
        id: giftCoin
        property real t: 1.0
        property real spotX: 0.5
        readonly property real landAt: 0.55        // t at which it touches down
        readonly property bool landed: t >= landAt && t < 1.0
        visible: !scene.reducedMotion && scene.sitMode && t < 1.0
        width: scene.coinSize; height: width
        x: scene.width * spotX - width / 2
        y: {
            if (t >= landAt)
                return scene.coinRestY - Math.abs(Math.sin((t - landAt) * 9)) * 10
                       * (1 - (t - landAt) / (1 - landAt));      // settle bounce
            var fall = t / landAt;
            return -height + fall * fall * (scene.coinRestY + height);
        }
        AnimatedImage {
            anchors.fill: parent
            source: "qrc:/scene/goldcoin.gif"
            playing: scene.live && giftCoin.visible
            opacity: giftCoin.t > 0.9 ? (1 - giftCoin.t) * 10 : 1
        }
        Repeater {
            model: 6
            Rectangle {
                readonly property real ang: (index / 6) * Math.PI * 2
                readonly property real burst: Math.max(0, Math.min(1,
                    (giftCoin.t - giftCoin.landAt) * 4))
                width: 3; height: 3; radius: 1.5; color: "#ffe9a3"
                visible: giftCoin.landed && burst < 1
                opacity: 1 - burst
                x: giftCoin.width / 2 - 1.5 + Math.cos(ang) * burst * 22
                y: giftCoin.height / 2 - 1.5 + Math.sin(ang) * burst * 22
            }
        }
        NumberAnimation { id: giftCoinAnim; target: giftCoin; property: "t"
                          from: 0; to: 1; duration: 3400 }
    }

    // The quiet screens get their own clock: every few seconds something small
    // happens, so the tab is never just a dog breathing.
    property real sitterHop: 0.0
    SequentialAnimation {
        id: sitterHopAnim
        NumberAnimation { target: scene; property: "sitterHop"; to: 9; duration: 180; easing.type: Easing.OutQuad }
        NumberAnimation { target: scene; property: "sitterHop"; to: 0; duration: 220; easing.type: Easing.InQuad }
        NumberAnimation { target: scene; property: "sitterHop"; to: 4; duration: 140; easing.type: Easing.OutQuad }
        NumberAnimation { target: scene; property: "sitterHop"; to: 0; duration: 120; easing.type: Easing.InQuad }
    }

    Timer {
        id: ambientTimer
        repeat: true
        running: scene.live && !scene.runMode
        interval: 5000
        onTriggered: {
            scene.ambientEvent();
            interval = 4200 + Math.floor(Math.random() * 7000);
        }
    }

    function ambientEvent() {
        var roll = Math.random();
        if (scene.moneyMode) {
            if (roll < 0.62) {
                flyingCoin.band = 0.16 + Math.random() * 0.30;
                flyingCoinAnim.duration = 2100 + Math.floor(Math.random() * 2200);
                flyingCoinAnim.restart();
            } else if (scene.night) {
                shootingStar.p = 1.0;
                starAnim.restart();
            }
            return;
        }
        // sit mode
        if (roll < 0.5) {
            // Beside him, on whichever side, and clear of the sun at 0.74.
            giftCoin.spotX = (Math.random() < 0.5) ? 0.30 + Math.random() * 0.12
                                                   : 0.58 + Math.random() * 0.10;
            giftCoinAnim.restart();
            sitterHopAnim.restart();                        // he notices
        } else if (roll < 0.72) {
            sitterHopAnim.restart();
        } else if (scene.night) {
            shootingStar.p = 1.0;
            starAnim.restart();
        }
    }

    // The doge, sitting this one out. Beat 0 only, and only on some laps.
    AnimatedImage {
        id: sitter
        source: "qrc:/scene/dog2.gif"
        playing: scene.live && visible
        width: 84; height: 67
        x: scene.width * (scene.sitMode ? 0.5 : 0.16) - (scene.sitMode ? width / 2 : 0)
        y: scene.groundY - height + 14 - scene.sitterHop
        visible: !scene.reducedMotion
                 && (scene.sitMode || (scene.runMode && scene.idleLap && scene.beat === 0))
        opacity: visible ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 400 } }
    }

    Item {
        id: rig
        visible: scene.runMode
        width: scene.dogWidth
        height: scene.dogHeight
        x: scene.reducedMotion ? 8 : scene.dogX
        y: scene.groundY - scene.dogHeight + 21 - scene.hopY + scene.crashDrop

        AnimatedImage {
            id: doge
            anchors.fill: parent
            source: "qrc:/scene/dog.gif"
            playing: scene.live && !scene.tripped
            // Freezing a run cycle leaves him in whatever mid-stride pose the
            // gif happened to be on, legs extended, which reads as airborne no
            // matter how he is rotated. Frame 4 has the legs tucked under the
            // body — the closest thing this sprite sheet has to a heap.
            onPlayingChanged: if (!playing && scene.tripped) currentFrame = 4
            // Face-plant: he tips over his own front paws — the pivot is the paw
            // on the grass line, not the middle of the sprite, or the snout
            // swings straight down through the ground. Then the face squashes
            // against the dirt and the body bends over it.
            readonly property real pivotX: rig.width * 0.86   // the snout
            readonly property real pivotY: rig.height - 14    // on the grass
            transform: [
                Scale {
                    origin.x: doge.pivotX; origin.y: doge.pivotY
                    xScale: scene.crashSquashX; yScale: scene.crashSquashY
                },
                Rotation {
                    origin.x: doge.pivotX; origin.y: doge.pivotY
                    angle: scene.crashRot
                }
            ]
        }

        // Dust off the snout on impact: thrown up and forward, then falling.
        Repeater {
            model: 12
            Rectangle {
                readonly property real ang: -Math.PI * (0.08 + 0.84 * (((index * 31) % 12) / 11))
                readonly property real spd: 30 + ((index * 37) % 26)
                width: 4 + (index % 5); height: width; radius: width / 2
                color: index % 3 === 0 ? "#fff6e4" : (index % 3 === 1 ? "#e8d6b4" : "#c9a878")
                visible: scene.dustT < 1.0
                opacity: (1.0 - scene.dustT) * 0.95
                x: rig.width * 0.90 - width / 2 + Math.cos(ang) * scene.dustT * spd
                y: rig.height - 12 + Math.sin(ang) * scene.dustT * spd * 0.45
                     + scene.dustT * scene.dustT * 26
            }
        }
    }

    // What he has to say about it. One bubble, used for both the wipeout and the
    // gloat; started and cleared by hand for the same reason as the +1.
    Item {
        id: bark
        readonly property real headX: rig.x + rig.width * 0.86
        width: barkText.implicitWidth + 18
        height: barkText.implicitHeight + 12
        x: Math.max(4, Math.min(scene.width - width - 4, headX - width * 0.35))
        y: Math.max(2, rig.y - height + 6)
        opacity: 0.0
        scale: 0.6
        transformOrigin: Item.BottomLeft
        visible: scene.runMode && opacity > 0.01

        Rectangle {
            anchors.fill: parent
            radius: 9
            color: "#fff6e2"
            border.color: "#2a1636"
            border.width: 2
        }
        // Tail, pointing back down at the doge.
        Rectangle {
            width: 10; height: 10
            color: "#fff6e2"
            border.color: "#2a1636"
            border.width: 2
            rotation: 45
            x: bark.width * 0.28
            y: bark.height - 6
            Rectangle {   // hide the seam the border draws across the bubble
                x: -2; y: -3; width: 14; height: 6; rotation: 0
                color: "#fff6e2"
            }
        }
        Text {
            id: barkText
            anchors.centerIn: parent
            text: scene.barkText
            color: "#2a1636"
            font.bold: true
            font.pixelSize: 15
        }

        SequentialAnimation {
            id: barkAnim
            ParallelAnimation {
                NumberAnimation { target: bark; property: "opacity"; to: 1.0; duration: 110 }
                NumberAnimation { target: bark; property: "scale"; from: 0.6; to: 1.0; duration: 300; easing.type: Easing.OutBack }
            }
            // A little shake, so it lands like a shout.
            SequentialAnimation {
                loops: 2
                NumberAnimation { target: bark; property: "rotation"; to: -3.5; duration: 70 }
                NumberAnimation { target: bark; property: "rotation"; to:  3.5; duration: 70 }
            }
            NumberAnimation { target: bark; property: "rotation"; to: 0; duration: 70 }
            PauseAnimation  { duration: 900 }
            ParallelAnimation {
                NumberAnimation { target: bark; property: "opacity"; to: 0.0; duration: 300 }
                NumberAnimation { target: bark; property: "scale"; to: 0.9; duration: 300 }
            }
        }
    }

    function say(text) {
        barkAnim.stop();
        bark.rotation = 0;
        scene.barkText = text;
        bark.opacity = 0.0;
        bark.scale = 0.6;
        barkAnim.start();
    }

    function hushBark() {
        barkAnim.stop();
        bark.opacity = 0.0;
        bark.scale = 0.6;
        bark.rotation = 0;
    }

    // Cleared the field and got across: say so, rather than letting the lap
    // just end and the next one start with a doge sitting down.
    Text {
        id: winner
        anchors.horizontalCenter: parent.horizontalCenter
        y: scene.height * 0.16
        text: scene.picked > 0 ? "WIN  +" + scene.picked : "WIN"
        color: "#8ff08a"
        style: Text.Outline
        styleColor: "#10240f"
        font.bold: true
        font.pixelSize: Math.max(20, Math.min(scene.height * 0.20, 34))
        opacity: (scene.runMode && scene.winShown) ? 1.0 : 0.0
        scale: scene.winShown ? 1.0 : 1.35
        Behavior on opacity { NumberAnimation { duration: 280 } }
        Behavior on scale   { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
    }

    Text {
        id: gameOver
        anchors.horizontalCenter: parent.horizontalCenter
        y: scene.height * 0.16
        text: "GAME OVER"
        color: "#ff6b6b"
        style: Text.Outline
        styleColor: "#1a0b1f"
        font.bold: true
        font.pixelSize: Math.max(20, Math.min(scene.height * 0.20, 34))
        opacity: (scene.runMode && scene.gameOverShown) ? 1.0 : 0.0
        scale: scene.gameOverShown ? 1.0 : 1.35
        Behavior on opacity { NumberAnimation { duration: 320 } }
        Behavior on scale   { NumberAnimation { duration: 420; easing.type: Easing.OutBack } }
    }

    // --- the crash ----------------------------------------------------------
    SequentialAnimation {
        id: crashAnim
        // Impact: he pitches onto his nose and the dust comes up. Small angle
        // on purpose — a shiba tripping is a face on the ground, not a cartwheel.
        ParallelAnimation {
            NumberAnimation { target: scene; property: "crashRot";     to: 26;   duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: scene; property: "crashDrop";    to: 21;   duration: 150; easing.type: Easing.InQuad }
            NumberAnimation { target: scene; property: "crashSquashX"; to: 1.07; duration: 150 }
            NumberAnimation { target: scene; property: "crashSquashY"; to: 0.93; duration: 150 }
            NumberAnimation { target: scene; property: "dustT"; from: 0; to: 1;  duration: 700; easing.type: Easing.OutQuad }
        }
        // The skid: his face carries on along the ground and stops.
        ParallelAnimation {
            NumberAnimation { target: scene; property: "crashSlide";   to: 26;   duration: 620; easing.type: Easing.OutQuad }
            NumberAnimation { target: scene; property: "crashRot";     to: 21;   duration: 380; easing.type: Easing.OutQuad }
            NumberAnimation { target: scene; property: "crashSquashX"; to: 1.03; duration: 380; easing.type: Easing.OutQuad }
            NumberAnimation { target: scene; property: "crashSquashY"; to: 0.97; duration: 380; easing.type: Easing.OutQuad }
        }
    }

    function resetCrash() {
        crashAnim.stop();
        scene.crashRot = 0.0;
        scene.crashSquashX = 1.0;
        scene.crashSquashY = 1.0;
        scene.crashDrop = 0.0;
        scene.crashSlide = 0.0;
        scene.dustT = 1.0;
    }

    // He got across with the field cleared.
    property bool winShown: false
    // GAME OVER lands a beat after the wipeout so the crash reads on its own.
    property bool gameOverShown: false
    Timer { id: gameOverTimer; interval: 460; repeat: false; onTriggered: scene.gameOverShown = true }

    onTrippedChanged: {
        if (scene.tripped) {
            crashAnim.restart();
            scene.say(scene.crashBarks[Math.floor(Math.random() * scene.crashBarks.length)]);
            gameOverTimer.restart();
            // No point running out the rest of a lap he is not going to finish.
            beatTimer.interval = 3200;
            beatTimer.restart();
        } else {
            scene.resetCrash();
        }
    }

    // --- the gloat ----------------------------------------------------------
    SequentialAnimation {
        id: hopAnim
        NumberAnimation { target: scene; property: "hopY"; to: 26; duration: 220; easing.type: Easing.OutQuad }
        NumberAnimation { target: scene; property: "hopY"; to: 0;  duration: 200; easing.type: Easing.InQuad }
        NumberAnimation { target: scene; property: "hopY"; to: 14; duration: 170; easing.type: Easing.OutQuad }
        NumberAnimation { target: scene; property: "hopY"; to: 0;  duration: 150; easing.type: Easing.InQuad }
    }

    function pick(worth) {
        scene.picked += worth;
        sparkAnim.restart();
        // Cleared the field: hop and gloat about it.
        if (scene.picked >= scene.coinCount + (scene.bonusIndex >= 0 ? 4 : 0)) {
            hopAnim.restart();
            scene.say(scene.winBarks[Math.floor(Math.random() * scene.winBarks.length)]);
        }
    }

    // --- the three beats ----------------------------------------------------
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
        onFinished: if (scene.live && !scene.tripped) scene.winShown = true
    }

    Timer { id: beatTimer; repeat: false; onTriggered: scene.advance() }

    // Beat 0 is the empty field, 1 is the coins coming out, 2 is the run.
    // Only "run" mode walks all three: money mode loops the money beat, and a
    // sitting doge needs no cycle at all.
    function startLap() {
        scene.stopEverything();

        scene.lapId += 1;
        scene.beat = 0;
        scene.runProgress = 0.0;
        scene.coinReveal = 0.0;
        scene.picked = 0;
        scene.hopY = 0.0;
        scene.sparkT = 1.0;
        scene.sitterHop = 0.0;
        flyingCoin.t = 1.0;
        giftCoin.t = 1.0;

        if (!scene.live)
            return;

        if (scene.sitMode) {
            scene.beat = 0;
            return;                       // nothing to schedule; he just sits
        }
        if (scene.moneyMode) {
            scene.moneyRolls = scene.rollMoney();
            scene.beat = 2;               // arms the bills through armFor()
            return;                       // each bill reschedules itself
        }

        scene.runBeat  = 6800 + Math.floor(Math.random() * 3000);
        scene.tripLap  = Math.random() < 0.17;
        scene.tripAt   = 0.34 + Math.random() * 0.34;
        scene.idleLap  = Math.random() < 0.45;
        scene.birdLap  = !scene.night && Math.random() < 0.5;
        scene.starLap  = scene.night && Math.random() < 0.4;
        scene.bonusIndex = (Math.random() < 0.3)
                           ? Math.floor(Math.random() * scene.coinCount) : -1;
        scene.moneyRolls = scene.rollMoney();

        beatTimer.interval = 3000 + Math.floor(Math.random() * 2000);   // 3-5s of empty field
        beatTimer.start();
    }

    function stopEverything() {
        beatTimer.stop();
        revealAnim.stop();
        runAnim.stop();
        hopAnim.stop();
        sparkAnim.stop();
        starTimer.stop();
        starAnim.stop();
        shootingStar.p = 1.0;
        // Order matters: park the run first. Clearing `tripLap` while the doge
        // is still frozen mid-field un-freezes him onto `runProgress`, which by
        // the end of a lap is 1.0 — he would teleport past every coin left
        // standing and "win" the lap he just lost.
        scene.beat = 0;
        scene.runProgress = 0.0;
        scene.coinReveal = 0.0;
        scene.tripLap = false;
        gameOverTimer.stop();
        scene.gameOverShown = false;
        scene.winShown = false;
        scene.hushBark();
        scene.resetCrash();
    }

    function advance() {
        if (!scene.live)
            return;
        if (!scene.runMode) {
            scene.startLap();
            return;
        }
        if (scene.beat === 0) {
            scene.beat = 1;
            revealAnim.start();
            beatTimer.interval = scene.coinCount * scene.revealStep + scene.settleBeat;
            beatTimer.start();
        } else if (scene.beat === 1) {
            scene.beat = 2;
            runAnim.start();
            if (scene.starLap) {
                shootingStar.p = 1.0;
                starTimer.interval = 600 + Math.floor(Math.random() * (scene.runBeat - 1800));
                starTimer.restart();
            }
            beatTimer.interval = scene.runBeat + 1400;   // holds on GAME OVER / the gloat
            beatTimer.start();
        } else {
            scene.startLap();
        }
    }

    // Called from the host when its tab is shown or hidden. A hidden scene is
    // parked at the start of a lap rather than left running against a 0px field.
    function restart() { scene.onScreen = true }
    function suspend() { scene.onScreen = false }

    onLiveChanged: {
        if (scene.live)
            scene.startLap();
        else
            scene.stopEverything();
    }

    Component.onCompleted: if (scene.live) scene.startLap();
}
