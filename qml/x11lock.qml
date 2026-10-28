// X11 stand-in for the ctOS greeter's Wayland locker.
// Copyright (C) 2026 Kikyo987
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Place this file in the root of a ctOS install, next to greeter/.
// The page itself is MainLayout from TSM-061/ctOS.

import QtQuick
import Quickshell
import Quickshell.X11

import qs.greeter.components
import qs.greeter.config

Scope {
    id: root

    readonly property string uiName: {
        const wanted = Settings.monitor
        const screens = Quickshell.screens
        for (let i = 0; i < screens.length; i++) {
            if (wanted && screens[i].name === wanted)
                return wanted
        }
        return screens.length ? screens[0].name : ""
    }

    Variants {
        model: Quickshell.screens

        XPanelWindow {
            id: panel

            property var modelData
            readonly property bool uiScreen: modelData.name === root.uiName

            screen: modelData
            color: "#0E0E0E"
            aboveWindows: true
            focusable: uiScreen
            exclusiveZone: 0
            exclusionMode: ExclusionMode.Ignore

            anchors {
                left: true
                right: true
                top: true
                bottom: true
            }

            function tag() {
                const backing = panel._backingWindow
                if (!backing)
                    return
                const title = panel.uiScreen ? "ctos-lock" : "ctos-lock-cover"
                if (backing.title !== title)
                    backing.title = title
            }

            onWindowConnected: panel.tag()
            Component.onCompleted: {
                panel.tag()
                console.log("ctos-x11-lock", modelData.name, panel.uiScreen ? "ui" : "cover")
            }

            Timer {
                interval: 50
                repeat: true
                running: true
                property int tries: 0
                onTriggered: {
                    panel.tag()
                    tries++
                    const backing = panel._backingWindow
                    if (tries > 40 || (backing && backing.title.length > 0))
                        running = false
                }
            }

            Image {
                anchors.fill: parent
                visible: !panel.uiScreen
                source: Qt.resolvedUrl("greeter/resources/lock.png")
                fillMode: Image.PreserveAspectCrop
            }

            Loader {
                anchors.fill: parent
                active: panel.uiScreen
                sourceComponent: MainLayout {}
            }
        }
    }
}
