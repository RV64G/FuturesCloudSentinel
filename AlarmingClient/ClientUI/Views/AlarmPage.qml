import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Alarm Monitor"
    property var theme: ApplicationWindow.window.theme

    background: Rectangle { color: theme.background }

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
                            color: theme.colorOutline
                            Layout.alignment: Qt.AlignBaseline
                        }
                        Label {
                            property double currentPrice: (backend.prices && backend.prices[modelData.symbol]) ? backend.prices[modelData.symbol] : 0
                            property double previousPrice: 0
                            property color priceColor: theme.colorOnSurfaceVariant // Default gray

                            onCurrentPriceChanged: {
                                if (currentPrice > previousPrice && previousPrice !== 0) {
                                    priceColor = "#F14C4C" // Red (Higher)
                                } else if (currentPrice < previousPrice && previousPrice !== 0) {
                                    priceColor = "#00C853" // Green (Lower)
                                } else {
                                    priceColor = theme.colorOnSurfaceVariant // Gray (Equal or First load)
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
                            text: modelData.type === "time" 
                                  ? ((modelData.trigger_time || ""))
                                  : ((modelData.max_price ? "≥ " + modelData.max_price : "") + 
                                     (modelData.max_price && modelData.min_price ? " | " : "") +
                                     (modelData.min_price ? "≤ " + modelData.min_price : ""))
                            
                            color: theme.primary
                        }
                    }
                    Label { text: modelData.type; font.pixelSize: 12; color: theme.colorOnSurfaceVariant }
                }
                highlighted: ListView.isCurrentItem
                onClicked: {
                    ListView.view.currentIndex = index
                    detailPanel.loadWarningForEdit(modelData)
                }
            }
            
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        // Right: Detail/Edit Panel
        Rectangle {
            id: detailPanel
            SplitView.fillWidth: true
            color: theme.surface // Use theme surface color
            
            property string currentOrderId: ""
            property bool isModifyMode: false
            property string originalSymbol: "" // To track if user changed symbol

            // Load warning data into the form for editing
            function loadWarningForEdit(warningItem) {
                currentOrderId = warningItem.order_id
                isModifyMode = true
                originalSymbol = warningItem.symbol
                
                instrumentCombo.editText = warningItem.symbol
                
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
                timeField.text = ""
                deleteBtn.visible = false
                alarmListView.currentIndex = -1 // Deselect list
            }

            Connections {
                target: backend
                function onWarningListChanged() {
                    // Force refresh if needed
                    alarmListView.model = backend.warningList
                    if (!detailPanel.isModifyMode) {
                        // Clear inputs only if we were adding (successful add)
                        // But if user was typing, maybe we shouldn't? 
                        // Let's clear to indicate success.
                        resetForm(false)
                    }
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
                            // Add safety check for backend
                            model: (typeof backend !== "undefined" && backend) ? backend.contractCodes : []
                            editable: true
                            Layout.fillWidth: true 
                            
                            onEditTextChanged: {
                                // If user changes symbol while in modify mode, switch to add mode
                                if (detailPanel.isModifyMode && editText !== detailPanel.originalSymbol) {
                                    detailPanel.resetForm(true) // Keep the text user just typed
                                }
                            }

                            onActivated: {
                                 // If user selects a different symbol while in modify mode, switch to add mode
                                if (detailPanel.isModifyMode && currentText !== detailPanel.originalSymbol) {
                                    detailPanel.resetForm(true) // Keep the selected text
                                }
                            }
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

                            property double currentPrice: (backend.prices && backend.prices[currentCode]) ? backend.prices[currentCode] : 0
                            property double previousPrice: 0
                            property color priceColor: theme.colorOnSurfaceVariant // Default gray

                            onCurrentPriceChanged: {
                                if (currentPrice > previousPrice && previousPrice !== 0) {
                                    priceColor = "#F14C4C" // Red (Higher)
                                } else if (currentPrice < previousPrice && previousPrice !== 0) {
                                    priceColor = "#00C853" // Green (Lower)
                                } else {
                                    // Keep previous color or reset to gray? 
                                    // Usually if price doesn't change, we keep the color or go gray.
                                    // Let's go gray for "Equal" as requested.
                                    priceColor = theme.colorOnSurfaceVariant 
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
                        placeholderText: "HH:mm:ss"
                        inputMask: "99:99:99"
                        Layout.fillWidth: true 
                        visible: typeCombo.currentIndex === 1
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
                                if (timeStr === "") {
                                    if (ApplicationWindow.window.tips) ApplicationWindow.window.tips.showMessage("请输入触发时间", "error")
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
                        onClicked: {
                            var symbol = instrumentCombo.currentText
                            backend.testTriggerAlert(symbol)
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
                                instrumentCombo.editText = ""
                                detailPanel.resetForm()
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true } // Spacer
            }
        }
    }
}
