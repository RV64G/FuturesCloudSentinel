import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    title: "Alarm Monitor"

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
                        Label { text: modelData.symbol; font.bold: true; Layout.fillWidth: true }
                        Label { 
                            text: (modelData.max_price ? "> " + modelData.max_price : "") + 
                                  (modelData.min_price ? " < " + modelData.min_price : "")
                            color: "red" 
                        }
                    }
                    Label { text: modelData.type; font.pixelSize: 12; color: "gray" }
                }
                highlighted: ListView.isCurrentItem
                onClicked: ListView.view.currentIndex = index
            }
            
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        // Right: Detail/Edit Panel
        Rectangle {
            id: detailPanel
            SplitView.fillWidth: true
            color: palette.window // Use system palette for adaptive background
            
            property string currentOrderId: ""
            property bool isModifyMode: false

            Connections {
                target: backend
                function onWarningListChanged() {
                    // Force refresh if needed, though binding should handle it
                    alarmListView.model = backend.warningList
                    detailPanel.checkExistingWarning(instrumentCombo.editText)
                }
            }

            function checkExistingWarning(symbolText) {
                // Simple extraction logic matching Backend's basic logic for UI check
                // Or just iterate and check if symbol matches
                // Since backend.warningList has pure codes (e.g. "IF2512"), 
                // and symbolText might be "300股指 (IF2512)", we need to be careful.
                // For simplicity, let's assume user selects from dropdown or types code.
                
                // Reset first
                currentOrderId = ""
                isModifyMode = false
                maxPriceField.text = ""
                minPriceField.text = ""
                deleteBtn.visible = false

                var code = symbolText
                if (code.indexOf("(") > -1) {
                    var start = code.lastIndexOf("(") + 1
                    var end = code.lastIndexOf(")")
                    code = code.substring(start, end)
                }
                code = code.trim()

                for (var i = 0; i < backend.warningList.length; i++) {
                    var item = backend.warningList[i]
                    if (item.symbol === code) {
                        detailPanel.currentOrderId = item.order_id
                        detailPanel.isModifyMode = true
                        if (item.max_price) maxPriceField.text = item.max_price
                        if (item.min_price) minPriceField.text = item.min_price
                        deleteBtn.visible = true
                        
                        // Sync selection in ListView
                        alarmListView.currentIndex = i
                        break
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 15

                Label { 
                    text: detailPanel.isModifyMode ? "Modify Alarm" : "Add Alarm"
                    font.pixelSize: 20 
                    font.bold: true 
                }

                GridLayout {
                    columns: 2
                    rowSpacing: 10
                    columnSpacing: 10

                    Label { text: "合约代码:" }
                    ComboBox { 
                        id: instrumentCombo
                        // Add safety check for backend
                        model: (typeof backend !== "undefined" && backend) ? backend.contractCodes : []
                        editable: true
                        Layout.fillWidth: true 
                        
                        // 搜索逻辑：输入文字 -> 过滤后端列表 -> 自动展开下拉框
                        onEditTextChanged: {
                            // backend.filterContractCodes(editText) // 暂时禁用搜索功能
                            detailPanel.checkExistingWarning(editText)
                            // if (count > 0 && !popup.visible) {
                            //     popup.open()
                            // }
                        }
                        
                        onActivated: {
                            detailPanel.checkExistingWarning(currentText)
                        }
                    }

                    Label { text: "上限价格 (>=):" }
                    TextField { 
                        id: maxPriceField
                        placeholderText: "价格上限"
                        Layout.fillWidth: true 
                        validator: DoubleValidator {}
                    }

                    Label { text: "下限价格 (<=):" }
                    TextField { 
                        id: minPriceField
                        placeholderText: "价格下限"
                        Layout.fillWidth: true 
                        validator: DoubleValidator {}
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
                            var max = parseFloat(maxPriceField.text) || 0
                            var min = parseFloat(minPriceField.text) || 0
                            
                            if (detailPanel.isModifyMode) {
                                backend.modifyPriceWarning(detailPanel.currentOrderId, max, min)
                            } else {
                                backend.addPriceWarning(instrumentCombo.currentText, max, min)
                            }
                        }
                    }

                    Button {
                        text: "重置"
                        onClicked: {
                            instrumentCombo.editText = ""
                            detailPanel.checkExistingWarning("")
                            backend.filterContractCodes("") // 恢复完整列表
                        }
                    }

                    Button { 
                        id: deleteBtn
                        text: "删除预警"
                        flat: true
                        visible: detailPanel.isModifyMode // 仅在修改模式(选中已存在预警)时显示
                        palette.buttonText: "red" 
                        onClicked: {
                            if (detailPanel.currentOrderId !== "") {
                                backend.deleteWarning(detailPanel.currentOrderId)
                                // 删除后重置界面
                                instrumentCombo.editText = ""
                                detailPanel.checkExistingWarning("")
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true } // Spacer
            }
        }
    }
}
