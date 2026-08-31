import QtQuick

// wowlet ambient scene — dusk sky, tiled grass, a running doge, bouncing WOW
// coins, drifting money. All motion gates on `scene.reducedMotion` (exposed by
// the host SceneBackend); frozen it is the reduced-motion still.
Rectangle {
    id: scene
    property bool reducedMotion: (typeof sceneCtx !== 'undefined' && sceneCtx) ? sceneCtx.reducedMotion : false

    gradient: Gradient {
        GradientStop { position: 0.0;  color: "#160d2b" }
        GradientStop { position: 0.5;  color: "#5b2f6e" }
        GradientStop { position: 0.82; color: "#c96aa0" }
        GradientStop { position: 1.0;  color: "#f2adc9" }
    }

    Rectangle { x: parent.width*0.74; y: parent.height*0.14; width: 62; height: 62; radius: 31; color: "#ffe9b0"; opacity: 0.9 }

    AnimatedImage {
        source: "qrc:/scene/flyingmoney.gif"; playing: !scene.reducedMotion
        width: 70; height: 64; y: 30; x: 120; opacity: 0.9
        NumberAnimation on x { running: !scene.reducedMotion; from: -80; to: scene.width; duration: 11000; loops: Animation.Infinite }
    }

    Row {
        anchors.bottom: parent.bottom
        Repeater { model: Math.ceil(scene.width/180)+1
            Image { source: "qrc:/scene/grass.png"; width: 180; height: 46; smooth: false } }
    }

    Repeater {
        model: 3
        AnimatedImage {
            source: "qrc:/scene/goldcoin.gif"; playing: !scene.reducedMotion
            width: 34; height: 34; x: 150 + index*170; y: scene.height*0.46
            SequentialAnimation on y {
                running: !scene.reducedMotion; loops: Animation.Infinite
                NumberAnimation { to: scene.height*0.36; duration: 1050 + index*170; easing.type: Easing.OutQuad }
                NumberAnimation { to: scene.height*0.5;  duration: 1050 + index*170; easing.type: Easing.InQuad }
            }
        }
    }

    AnimatedImage {
        source: "qrc:/scene/dog.gif"; playing: !scene.reducedMotion
        width: 104; height: 83; y: scene.height - 46 - 62; x: 70
        NumberAnimation on x { running: !scene.reducedMotion; from: -110; to: scene.width; duration: 6500; loops: Animation.Infinite }
    }
}
