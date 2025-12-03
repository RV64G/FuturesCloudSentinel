import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: loginPage
    title: "登录"
    
    property var theme: ApplicationWindow.window ? ApplicationWindow.window.theme : null
    background: Rectangle { color: theme ? theme.background : "#ffffff" }

    signal loginSuccess()

    property var tips: ApplicationWindow.window ? ApplicationWindow.window.tips : null
    property bool isBusy: false

    Component.onCompleted: {
        var creds = backend.loadCredentials()
        // creds is a QVariantMap: { "rememberUser": bool, "autoLogin": bool, "username": string, "password": string }
        
        if (creds.rememberUser) {
            userIdField.text = creds.username
            rememberUser.checked = true
            
            if (creds.autoLogin) {
                passwordField.text = creds.password
                autoLogin.checked = true
                
                // Auto login
                loginPage.isBusy = true
                backend.login(creds.username, creds.password, brokerIdField.text, loginPage.defaultFrontAddr)
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: 300
        spacing: 15
        enabled: !isBusy

        Label {
            text: "期货云哨兵"
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 20
        }

        TextField {
            id: brokerIdField
            placeholderText: "经纪商ID"
            Layout.fillWidth: true
            text: "9999"
        }

        TextField {
            id: userIdField
            placeholderText: "用户名"
            Layout.fillWidth: true
        }

        TextField {
            id: passwordField
            placeholderText: "密码"
            echoMode: TextInput.Password
            Layout.fillWidth: true
        }

        TextField {
            id: serverIpField
            placeholderText: "服务器IP (调试)"
            Layout.fillWidth: true
            visible: backend ? backend.isDebugUI : false
            text: backend ? backend.serverAddress : ""
            onEditingFinished: if (backend) backend.serverAddress = text
        }

        TextField {
            id: serverPortField
            placeholderText: "服务器端口 (调试)"
            Layout.fillWidth: true
            visible: backend ? backend.isDebugUI : false
            text: backend ? backend.serverPort.toString() : "8888"
            validator: IntValidator { bottom: 1; top: 65535 }
            onEditingFinished: if (backend) backend.serverPort = parseInt(text) || 8888
        }

        RowLayout {
            Layout.fillWidth: true
            CheckBox {
                id: rememberUser
                text: "记住用户名"
                onCheckedChanged: {
                    if (!checked) {
                        autoLogin.checked = false
                    }
                }
            }
            CheckBox {
                id: autoLogin
                text: "自动登录"
                enabled: rememberUser.checked
            }
        }

        // Front address removed: server connection address is no longer user-input.

        Button {
            text: "登录"
            highlighted: true
            Layout.fillWidth: true
            Layout.topMargin: 10
            onClicked: {
                if (userIdField.text === "" || passwordField.text === "") {
                    if (tips) tips.showMessage("请输入用户名和密码", "error")
                    return
                }
                loginPage.isBusy = true
                backend.saveCredentials(userIdField.text, passwordField.text, rememberUser.checked, autoLogin.checked)
                backend.login(userIdField.text, passwordField.text, brokerIdField.text, loginPage.defaultFrontAddr)
            }
        }

        Button {
            text: "注册"
            flat: true
            Layout.fillWidth: true
            onClicked: {
                registerPopup.open()
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: loginPage.isBusy
        visible: running
    }

    Popup {
        id: registerPopup
        width: 300
        height: 350
        modal: true
        focus: true
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: ApplicationWindow.window ? ApplicationWindow.window.color : "#ffffff"
            border.color: "#cccccc"
            radius: 5
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - 40
            spacing: 15

            Label {
                text: "创建账户"
                font.pixelSize: 20
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            TextField {
                id: regUsernameField
                placeholderText: "用户名"
                Layout.fillWidth: true
            }

            TextField {
                id: regPasswordField
                placeholderText: "密码"
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }
            
            TextField {
                id: regConfirmPasswordField
                placeholderText: "确认密码"
                echoMode: TextInput.Password
                Layout.fillWidth: true
            }

            Button {
                text: "注册"
                highlighted: true
                Layout.fillWidth: true
                Layout.topMargin: 10
                onClicked: {
                    if (regPasswordField.text !== regConfirmPasswordField.text) {
                        if (tips) tips.showMessage("两次密码不一致", "error")
                        return
                    }
                    if (regUsernameField.text === "" || regPasswordField.text === "") {
                        if (tips) tips.showMessage("请填写所有字段", "error")
                        return
                    }
                    
                    backend.registerUser(regUsernameField.text, regPasswordField.text)
                }
            }
        }
    }

    Connections {
        target: backend
        function onLoginSuccess() {
            loginPage.isBusy = false
            if (tips) tips.showMessage("登录成功！", "success")
            loginPage.loginSuccess()
        }
        function onLoginFailed(message) {
            loginPage.isBusy = false
            if (tips) tips.showMessage(message, "error")
        }
        function onRegisterSuccess() {
            if (tips) tips.showMessage("注册成功！", "success")
            registerPopup.close()
        }
        function onRegisterFailed(message) {
            if (tips) tips.showMessage(message, "error")
        }
    }
}
