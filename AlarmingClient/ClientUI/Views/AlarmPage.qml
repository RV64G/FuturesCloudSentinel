import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "预警监控"
    property var theme: ApplicationWindow.window ? ApplicationWindow.window.theme : null

    background: Rectangle { color: theme ? theme.background : "#ffffff" }

    Component.onCompleted: backend.queryWarnings()

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left: Alarm List
        ListView {
            id: alarmListView
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 200
            clip: true
            
            model: backend.warningList

            delegate: ItemDelegate {
                width: ListView.view.width
                contentItem: ColumnLayout {
                    RowLayout {
                        Label { text: modelData.symbol; font.bold: true }
                        Label { 
                            text: modelData.contract_name ? modelData.contract_name : ""
                            font.pixelSize: 12
                            color: theme ? theme.colorOutline : "#888888"
                            Layout.alignment: Qt.AlignBaseline
                        }
                        Label {
                            property double currentPrice: (backend && backend.prices && backend.prices[modelData.symbol]) ? backend.prices[modelData.symbol] : 0
                            property double previousPrice: 0
                            property color priceColor: theme ? theme.colorOnSurfaceVariant : "#666666"

                            onCurrentPriceChanged: {
                                if (currentPrice > previousPrice && previousPrice !== 0) {
                                    priceColor = "#F14C4C" // Red (Higher)
                                } else if (currentPrice < previousPrice && previousPrice !== 0) {
                                    priceColor = "#00C853" // Green (Lower)
                                } else {
                                    priceColor = theme ? theme.colorOnSurfaceVariant : "#666666"
                                }
                                previousPrice = currentPrice
                            }

                            text: currentPrice > 0 ? currentPrice.toFixed(1) : "--"
                            color: priceColor
                            font.bold: true
                            visible: text !== "--"
                        }
                        Item { Layout.fillWidth: true }
                        Label { 
                            // 价格预警显示上下限
                            text: modelData.type === "time" 
                                  ? ""
                                  : ((modelData.max_price ? "≥ " + modelData.max_price : "") + 
                                     (modelData.max_price && modelData.min_price ? " | " : "") +
                                     (modelData.min_price ? "≤ " + modelData.min_price : ""))
                            visible: modelData.type !== "time"
                            color: theme ? theme.primary : "#0078d4"
                        }
                    }
                    RowLayout {
                        spacing: 8
                        Label { 
                            text: modelData.type === "time" ? "时间预警" : "价格预警"
                            font.pixelSize: 12
                            color: theme ? theme.colorOnSurfaceVariant : "#666666" 
                        }
                        Label {
                            text: modelData.status === "triggered" ? "已触发" : "未触发"
                            font.pixelSize: 12
                            color: modelData.status === "triggered" ? "#F14C4C" : (theme ? theme.colorOnSurfaceVariant : "#666666")
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            // 时间预警在这里显示触发时间，只显示时间部分
                            text: modelData.type === "time" && modelData.trigger_time 
                                  ? modelData.trigger_time.substring(5)  // 去掉年份，显示 MM-DD HH:mm:ss
                                  : ""
                            visible: modelData.type === "time" && modelData.trigger_time
                            font.pixelSize: 12
                            color: theme ? theme.primary : "#0078d4"
                        }
                    }
                }
                highlighted: ListView.isCurrentItem
                onClicked: {
                    alarmListView.currentIndex = index
                    detailPanel.loadWarningForEdit(modelData)
                }
            }
            
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        // Right: Detail/Edit Panel
        Rectangle {
            id: detailPanel
            SplitView.fillWidth: true
            color: theme ? theme.surface : "#f5f5f5"
            
            property string currentOrderId: ""
            property bool isModifyMode: false
            property string originalSymbol: "" // To track if user changed symbol

            // Load warning data into the form for editing
            function loadWarningForEdit(warningItem) {
                currentOrderId = warningItem.order_id
                isModifyMode = true
                originalSymbol = warningItem.symbol
                
                // 查找完整的显示文本 "名称 (代码)"
                var displayText = warningItem.symbol
                if (warningItem.contract_name && warningItem.contract_name !== warningItem.symbol) {
                    displayText = warningItem.contract_name + " (" + warningItem.symbol + ")"
                }
                
                // 在 model 中查找匹配项
                var foundIndex = -1
                for (var i = 0; i < instrumentCombo.count; i++) {
                    var itemText = instrumentCombo.textAt(i)
                    if (itemText.indexOf("(" + warningItem.symbol + ")") !== -1) {
                        foundIndex = i
                        displayText = itemText
                        break
                    }
                }
                
                if (foundIndex >= 0) {
                    instrumentCombo.currentIndex = foundIndex
                    instrumentCombo.editText = displayText  // 必须手动设置 editText
                } else {
                    instrumentCombo.editText = displayText
                }
                
                if (warningItem.type === "time") {
                    typeCombo.currentIndex = 1 // Time
                    timeField.text = warningItem.trigger_time ? warningItem.trigger_time : ""
                    maxPriceField.text = ""
                    minPriceField.text = ""
                } else {
                    typeCombo.currentIndex = 0 // Price
                    maxPriceField.text = warningItem.max_price ? warningItem.max_price : ""
                    minPriceField.text = warningItem.min_price ? warningItem.min_price : ""
                    timeField.text = ""
                }
                deleteBtn.visible = true
            }

            function resetForm(keepInput) {
                currentOrderId = ""
                isModifyMode = false
                originalSymbol = ""
                if (!keepInput) {
                    instrumentCombo.editText = ""
                }
                maxPriceField.text = ""
                minPriceField.text = ""
                // 重置为价格预警，时间字段清空（切换到时间预警时会自动填充）
                typeCombo.currentIndex = 0
                timeField.text = ""
                deleteBtn.visible = false
                alarmListView.currentIndex = -1 // Deselect list
            }

            Connections {
                target: backend
                function onWarningListChanged() {
                    alarmListView.model = backend.warningList
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                Label { 
                    text: detailPanel.isModifyMode ? "修改预警" : "添加预警"
                    font.pixelSize: 20 
                    font.bold: true 
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 10

                    Label { text: "合约代码:" }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox { 
                            id: instrumentCombo
                            model: backend ? backend.contractCodes : []
                            editable: true
                            Layout.fillWidth: true
                            currentIndex: 0
                        }
                        Label {
                            property string currentCode: {
                                var text = instrumentCombo.currentText
                                var match = text.match(/\((.*?)\)/)
                                return match ? match[1] : text
                            }
                            onCurrentCodeChanged: {
                                if (currentCode && currentCode !== "") {
                                    backend.subscribe(currentCode)
                                    // Reset previous price when switching contracts to avoid wrong color comparison
                                    previousPrice = 0 
                                }
                            }

                            property double currentPrice: (backend && backend.prices && backend.prices[currentCode]) ? backend.prices[currentCode] : 0
                            property double previousPrice: 0
                            property color priceColor: theme ? theme.colorOnSurfaceVariant : "#666666"

                            onCurrentPriceChanged: {
                                if (currentPrice > previousPrice && previousPrice !== 0) {
                                    priceColor = "#F14C4C" // Red (Higher)
                                } else if (currentPrice < previousPrice && previousPrice !== 0) {
                                    priceColor = "#00C853" // Green (Lower)
                                } else {
                                    priceColor = theme ? theme.colorOnSurfaceVariant : "#666666"
                                }
                                previousPrice = currentPrice
                            }

                            text: currentPrice > 0 ? currentPrice.toFixed(1) : ""
                            font.bold: true
                            color: priceColor
                        }
                    }

                    Label { text: "预警类型:" }
                    ComboBox {
                        id: typeCombo
                        model: ["价格预警", "时间预警"]
                        Layout.fillWidth: true
                        
                        // 切换到时间预警时自动填充今天日期
                        onCurrentIndexChanged: {
                            if (currentIndex === 1) {
                                // 检查时间字段是否为空或只有占位符
                                // inputMask 会让空值显示为 "    -  -     :  :  " 类似格式
                                var timeText = timeField.text.replace(/[\s\-:]/g, '')  // 移除空格、横杠、冒号
                                if (timeText === "" || !/\d/.test(timeText)) {
                                    var today = new Date()
                                    var yyyy = today.getFullYear()
                                    var mm = String(today.getMonth() + 1).padStart(2, '0')
                                    var dd = String(today.getDate()).padStart(2, '0')
                                    timeField.text = yyyy + "-" + mm + "-" + dd + " 00:00:00"
                                }
                            }
                        }
                    }

                    Label { 
                        text: "上限价格:" 
                        visible: typeCombo.currentIndex === 0
                    }
                    TextField { 
                        id: maxPriceField
                        placeholderText: "高于此价格则预警"
                        placeholderTextColor: "#AAAAAA"
                        Layout.fillWidth: true 
                        visible: typeCombo.currentIndex === 0
                        validator: DoubleValidator {}
                    }

                    Label { 
                        text: "下限价格:" 
                        visible: typeCombo.currentIndex === 0
                    }
                    TextField { 
                        id: minPriceField
                        placeholderText: "低于此价格则预警"
                        placeholderTextColor: "#AAAAAA"
                        Layout.fillWidth: true 
                        visible: typeCombo.currentIndex === 0
                        validator: DoubleValidator {}
                    }

                    Label { 
                        text: "触发时间:" 
                        visible: typeCombo.currentIndex === 1
                    }
                    TextField { 
                        id: timeField
                        placeholderText: "YYYY-MM-DD HH:mm:ss"
                        inputMask: "9999-99-99 99:99:99"
                        Layout.fillWidth: true 
                        visible: typeCombo.currentIndex === 1
                        
                        // 自动填充今天日期
                        Component.onCompleted: {
                            var today = new Date()
                            var yyyy = today.getFullYear()
                            var mm = String(today.getMonth() + 1).padStart(2, '0')
                            var dd = String(today.getDate()).padStart(2, '0')
                            text = yyyy + "-" + mm + "-" + dd + " 00:00:00"
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button { 
                        text: detailPanel.isModifyMode ? "修改预警" : "添加预警"
                        highlighted: true 
                        Layout.fillWidth: true
                        onClicked: {
                            var symbol = instrumentCombo.currentText
                            if (symbol === "") {
                                if (ApplicationWindow.window.tips) ApplicationWindow.window.tips.showMessage("请输入合约代码", "error")
                                return
                            }

                            if (typeCombo.currentIndex === 0) { // Price
                                var max = parseFloat(maxPriceField.text)
                                var min = parseFloat(minPriceField.text)
                                
                                if (isNaN(max) && isNaN(min)) {
                                    if (ApplicationWindow.window.tips) ApplicationWindow.window.tips.showMessage("请至少输入一个价格条件", "error")
                                    return
                                }
                                
                                // Treat NaN as 0 for backend, or handle logic there. 
                                // Backend expects double. 0 usually means no limit if logic handles it, 
                                // but let's pass 0 if empty.
                                max = isNaN(max) ? 0 : max
                                min = isNaN(min) ? 0 : min

                                if (detailPanel.isModifyMode) {
                                    backend.modifyPriceWarning(detailPanel.currentOrderId, max, min)
                                } else {
                                    backend.addPriceWarning(symbol, max, min)
                                }
                            } else { // Time
                                var timeStr = timeField.text.trim()
                                console.log("[AlarmPage] Time string:", timeStr)
                                // inputMask 会用空格填充未输入的位置，检查是否完整
                                if (timeStr === "" || timeStr.indexOf(" ") === 0 || timeStr.length < 19) {
                                    if (ApplicationWindow.window.tips) ApplicationWindow.window.tips.showMessage("请输入完整的触发时间", "error")
                                    return
                                }
                                // 验证格式是否正确 (简单检查)
                                var regex = /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/
                                if (!regex.test(timeStr)) {
                                    if (ApplicationWindow.window.tips) ApplicationWindow.window.tips.showMessage("时间格式不正确，请使用 YYYY-MM-DD HH:mm:ss", "error")
                                    return
                                }

                                if (detailPanel.isModifyMode) {
                                    backend.modifyTimeWarning(detailPanel.currentOrderId, timeStr)
                                } else {
                                    backend.addTimeWarning(symbol, timeStr)
                                }
                            }
                        }
                    }

                    Button {
                        text: "模拟触发"
                        visible: backend ? backend.isSimulateServer : false  // 仅在模拟服务器模式显示
                        onClicked: {
                            var symbol = instrumentCombo.currentText
                            if (backend) backend.testTriggerAlert(symbol)
                        }
                    }

                    Button { 
                        id: deleteBtn
                        text: "删除预警"
                        flat: true
                        visible: detailPanel.isModifyMode 
                        palette.buttonText: "red" 
                        onClicked: {
                            if (detailPanel.currentOrderId !== "") {
                                backend.deleteWarning(detailPanel.currentOrderId)
                                detailPanel.resetForm(true)  // 保留当前合约代码
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true } // Spacer
            }
        }
    }
}
