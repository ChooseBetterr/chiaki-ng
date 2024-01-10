import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaki4deck

Item {
    id: view

    property bool sessionError: false
    property bool sessionLoading: true
    property list<Item> restoreFocusItems

    function grabInput(item) {
        Chiaki.window.grabInput();
        restoreFocusItems.push(Window.window.activeFocusItem);
        if (item)
            item.forceActiveFocus();
    }

    function releaseInput() {
        Chiaki.window.releaseInput();
        let item = restoreFocusItems.pop();
        if (item && item.visible)
            item.forceActiveFocus();
    }

    StackView.onActivating: Chiaki.window.keepVideo = true
    StackView.onDeactivated: Chiaki.window.keepVideo = false

    Rectangle {
        id: loadingView
        anchors.fill: parent
        color: "black"
        opacity: sessionError || sessionLoading ? 1.0 : 0.0
        visible: opacity

        Behavior on opacity { NumberAnimation { duration: 250 } }

        Item {
            anchors {
                top: parent.verticalCenter
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            BusyIndicator {
                id: spinner
                anchors.centerIn: parent
                width: 70
                height: width
                visible: sessionLoading
            }

            Label {
                id: errorTitleLabel
                anchors {
                    bottom: spinner.top
                    horizontalCenter: spinner.horizontalCenter
                }
                font.pixelSize: 24
                visible: text
                onVisibleChanged: if (visible) view.grabInput(errorTitleLabel)
                Keys.onReturnPressed: root.showMainView()
                Keys.onEscapePressed: root.showMainView()
            }

            Label {
                id: errorTextLabel
                anchors {
                    top: errorTitleLabel.bottom
                    horizontalCenter: errorTitleLabel.horizontalCenter
                    topMargin: 10
                }
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 20
                visible: text
            }
        }
    }

    ColumnLayout {
        id: cantDisplayMessage
        anchors.centerIn: parent
        opacity: Chiaki.window.hasVideo && Chiaki.session && Chiaki.session.cantDisplay ? 1.0 : 0.0
        visible: opacity
        spacing: 30

        Behavior on opacity { NumberAnimation { duration: 250 } }

        onVisibleChanged: {
            if (visible) {
                menuView.close();
                view.grabInput(goToHomeButton);
            } else {
                view.releaseInput();
            }
        }

        Label {
            Layout.alignment: Qt.AlignCenter
            text: qsTr("The screen contains content that can't be displayed using Remote Play.")
        }

        Button {
            id: goToHomeButton
            Layout.alignment: Qt.AlignCenter
            Layout.preferredHeight: 60
            text: qsTr("Go to Home Screen")
            Material.background: activeFocus ? parent.Material.accent : undefined
            Material.roundedScale: Material.SmallScale
            onClicked: Chiaki.sessionGoHome()
            Keys.onReturnPressed: clicked()
            Keys.onEscapePressed: clicked()
        }
    }

    RoundButton {
        anchors {
            right: parent.right
            top: parent.top
            margins: 40
        }
        icon.source: "qrc:/icons/discover-off-24px.svg"
        icon.width: 50
        icon.height: 50
        padding: 20
        checked: true
        opacity: networkIndicatorTimer.running ? 0.7 : 0.0
        visible: opacity
        Material.background: Material.accent

        Behavior on opacity { NumberAnimation { duration: 400 } }

        Timer {
            id: networkIndicatorTimer
            interval: 400
        }
    }

    Item {
        id: menuView
        property bool closing: false
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        height: 200
        opacity: 0.0
        visible: opacity
        enabled: visible
        onVisibleChanged: {
            if (visible)
                view.grabInput(closeButton);
            closing = false;
        }

        Behavior on opacity { NumberAnimation { duration: 250 } }

        function toggle() {
            if (visible)
                close();
            else
                opacity = 1.0;
        }

        function close() {
            if (!visible || closing)
                return;
            closing = true;
            opacity = 0.0;
            view.releaseInput();
        }

        Canvas {
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                let ctx = getContext("2d");
                let gradient = ctx.createLinearGradient(0, 0, 0, height);
                gradient.addColorStop(0.0, Qt.rgba(0.0, 0.0, 0.0, 0.0));
                gradient.addColorStop(0.7, Qt.rgba(0.5, 0.5, 0.5, 0.7));
                gradient.addColorStop(1.0, Qt.rgba(0.5, 0.5, 0.5, 0.9));
                ctx.fillStyle = gradient;
                ctx.fillRect(0, 0, width, height);
            }
        }

        RowLayout {
            anchors {
                left: parent.left
                bottom: parent.bottom
                leftMargin: 30
                bottomMargin: 40
            }
            spacing: 0

            ToolButton {
                id: closeButton
                Layout.rightMargin: 40
                text: "×"
                padding: 20
                font.pixelSize: 50
                down: activeFocus
                onClicked: {
                    if (Chiaki.session)
                        Chiaki.window.close();
                    else
                        root.showMainView();
                }
                KeyNavigation.right: muteButton
                Keys.onReturnPressed: clicked()
                Keys.onEscapePressed: menuView.close()
            }

            ToolButton {
                id: muteButton
                Layout.rightMargin: 40
                text: qsTr("Mic")
                padding: 20
                checkable: true
                enabled: Chiaki.session && Chiaki.session.connected
                checked: Chiaki.session && !Chiaki.session.muted
                onToggled: Chiaki.session.muted = !Chiaki.session.muted
                KeyNavigation.left: closeButton
                KeyNavigation.right: zoomButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuView.close()
            }

            ToolButton {
                id: zoomButton
                text: qsTr("Zoom")
                padding: 20
                checkable: true
                checked: Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom
                onToggled: Chiaki.window.videoMode = Chiaki.window.videoMode == ChiakiWindow.VideoMode.Zoom ? ChiakiWindow.VideoMode.Normal : ChiakiWindow.VideoMode.Zoom
                KeyNavigation.left: muteButton
                KeyNavigation.right: stretchButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuView.close()
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                id: stretchButton
                Layout.rightMargin: 50
                text: qsTr("Stretch")
                padding: 20
                checkable: true
                checked: Chiaki.window.videoMode == ChiakiWindow.VideoMode.Stretch
                onToggled: Chiaki.window.videoMode = Chiaki.window.videoMode == ChiakiWindow.VideoMode.Stretch ? ChiakiWindow.VideoMode.Normal : ChiakiWindow.VideoMode.Stretch
                KeyNavigation.left: zoomButton
                KeyNavigation.right: defaultButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuView.close()
            }

            ToolButton {
                id: defaultButton
                text: qsTr("Default")
                padding: 20
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.Default
                onToggled: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.Default
                KeyNavigation.left: stretchButton
                KeyNavigation.right: highQualityButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuView.close()
            }

            ToolSeparator {
                Layout.leftMargin: -10
                Layout.rightMargin: -10
            }

            ToolButton {
                id: highQualityButton
                text: qsTr("High Quality")
                padding: 20
                checkable: true
                checked: Chiaki.window.videoPreset == ChiakiWindow.VideoPreset.HighQuality
                onToggled: Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQuality
                KeyNavigation.left: defaultButton
                Keys.onReturnPressed: toggled()
                Keys.onEscapePressed: menuView.close()
            }
        }

        Label {
            anchors {
                right: consoleNameLabel.right
                bottom: consoleNameLabel.top
                bottomMargin: 5

            }
            text: "Mbps"
            font.pixelSize: 18
            visible: Chiaki.session

            Label {
                anchors {
                    right: parent.left
                    baseline: parent.baseline
                    rightMargin: 5
                }
                text: visible ? Chiaki.session.measuredBitrate.toFixed(1) : ""
                color: Material.accent
                font.bold: true
                font.pixelSize: 28
            }
        }

        Label {
            id: consoleNameLabel
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: 50
            }
            text: {
                if (!Chiaki.session)
                    return "";
                if (Chiaki.session.connected)
                    return qsTr("Connected to <b>%1</b>").arg(Chiaki.session.host);
                return qsTr("Connecting to <b>%1</b>").arg(Chiaki.session.host);
            }
        }
    }

    Popup {
        id: sessionStopDialog
        property int closeAction: 0
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        modal: true
        padding: 30
        onAboutToShow: {
            closeAction = 0;
        }
        onClosed: {
            view.releaseInput();
            if (closeAction)
                Chiaki.stopSession(closeAction == 1);
        }

        ColumnLayout {
            Label {
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Disconnect Session")
                font.bold: true
                font.pixelSize: 24
            }

            Label {
                Layout.topMargin: 10
                Layout.alignment: Qt.AlignCenter
                text: qsTr("Do you want the Console to go into sleep mode?")
                font.pixelSize: 20
            }

            RowLayout {
                Layout.topMargin: 30
                Layout.alignment: Qt.AlignCenter
                spacing: 30

                Button {
                    id: sleepButton
                    Layout.preferredWidth: 200
                    Layout.minimumHeight: 80
                    Layout.maximumHeight: 80
                    text: qsTr("Sleep")
                    font.pixelSize: 24
                    Material.roundedScale: Material.SmallScale
                    Material.background: activeFocus ? parent.Material.accent : undefined
                    KeyNavigation.right: noButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: sessionStopDialog.close()
                    onVisibleChanged: if (visible) view.grabInput(sleepButton)
                    onClicked: {
                        sessionStopDialog.closeAction = 1;
                        sessionStopDialog.close();
                    }
                }

                Button {
                    id: noButton
                    Layout.preferredWidth: 200
                    Layout.minimumHeight: 80
                    Layout.maximumHeight: 80
                    text: qsTr("No")
                    font.pixelSize: 24
                    Material.roundedScale: Material.SmallScale
                    Material.background: activeFocus ? parent.Material.accent : undefined
                    KeyNavigation.left: sleepButton
                    Keys.onReturnPressed: clicked()
                    Keys.onEscapePressed: sessionStopDialog.close()
                    onClicked: {
                        sessionStopDialog.closeAction = 2;
                        sessionStopDialog.close();
                    }
                }
            }
        }
    }

    Dialog {
        id: sessionPinDialog
        parent: Overlay.overlay
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - height) / 2)
        title: qsTr("Console Login PIN")
        modal: true
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            standardButton(Dialog.Ok).enabled = Qt.binding(function() {
                return pinField.acceptableInput;
            });
            view.grabInput(pinField);
        }
        onClosed: view.releaseInput()
        onAccepted: Chiaki.enterPin(pinField.text)
        onRejected: Chiaki.stopSession(false)
        Material.roundedScale: Material.MediumScale

        TextField {
            id: pinField
            implicitWidth: 200
            validator: RegularExpressionValidator { regularExpression: /[0-9]{4}/ }
        }
    }

    Timer {
        id: closeTimer
        interval: 2000
        onTriggered: root.showMainView()
    }

    Connections {
        target: Chiaki

        function onSessionChanged() {
            if (!Chiaki.session) {
                if (errorTitleLabel.text)
                    closeTimer.start();
                else
                    root.showMainView();
            }
        }

        function onSessionError(title, text) {
            sessionError = true;
            sessionLoading = false;
            errorTitleLabel.text = title;
            errorTextLabel.text = text;
            closeTimer.start();
        }

        function onSessionPinDialogRequested() {
            if (sessionPinDialog.opened)
                return;
            menuView.close();
            sessionPinDialog.open();
        }

        function onSessionStopDialogRequested() {
            if (sessionStopDialog.opened)
                return;
            menuView.close();
            sessionStopDialog.open();
        }
    }

    Connections {
        target: Chiaki.window

        function onHasVideoChanged() {
            if (Chiaki.window.hasVideo)
                sessionLoading = false;
        }

        function onCorruptedFramesChanged() {
            if (Chiaki.window.corruptedFrames > 3)
                networkIndicatorTimer.restart();
        }

        function onMenuRequested() {
            if (sessionPinDialog.opened || sessionStopDialog.opened)
                return;
            menuView.toggle();
        }
    }
}
