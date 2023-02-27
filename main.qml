import QtQuick 2.0
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.labs.platform 1.1
import VTK 8.2


Window {
    id: root
    property bool utv: false
    width: 1200
    height: 900
    color: "lightblue"

    Rectangle {
        id : vtk_container
        color: "pink"
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: parent.bottom
            margins: 20
        }

        RowLayout {
            id: row_layout
            anchors.fill: parent
            spacing: 5

            Rectangle {
                color: "grey"
                Layout.fillWidth: true
                Layout.preferredWidth: parent.width * 0.2
                Layout.preferredHeight: parent.height

                Slider {
                    id: slider_window
                    from: 0
                    value: to
                    to: mri_data_provider.windowRange
                    orientation: Qt.Vertical
                    anchors {
                        left: parent.left
                        right: parent.horizontalCenter
                        top: parent.top
                        bottom: parent.verticalCenter
                    }
                    onMoved: {
                        mri_data_provider.setWindow(value)
                    }
                }

                Slider {
                    id: slider_level
                    from: 0
                    value: to / 2
                    to: mri_data_provider.windowRange
                    orientation: Qt.Vertical
                    anchors {
                        left: parent.horizontalCenter
                        right: parent.right
                        top: parent.top
                        bottom: parent.verticalCenter
                    }
                    onMoved: {
                        mri_data_provider.setLevel(value)
                    }
                }

                Button {
                    id: button_i
                    text: "Инион"
                    anchors {
                        left: parent.left
                        right: parent.horizontalCenter
                        bottom: button_tl.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.pickBasePoint(0)
                }

                Button {
                    id: button_n
                    text: "Насион"
                    anchors {
                        left: parent.horizontalCenter
                        right: parent.right
                        bottom: button_tr.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.pickBasePoint(1)
                }

                Button {
                    id: button_tl
                    text: "Козелок Л"
                    anchors {
                        left: parent.left
                        right: parent.horizontalCenter
                        bottom: button_points.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.pickBasePoint(2)
                }

                Button {
                    id: button_tr
                    text: "Козелок П"
                    anchors {
                        left: parent.horizontalCenter
                        right: parent.right
                        bottom: button_points.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.pickBasePoint(3)
                }

                Button {
                    id: button_points
                    text: "Построить точки 10-20"
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: button_nav_points.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.buildPoints10_20()
                }

                Button {
                    id: button_nav_points
                    text: "Навигация по точкам"
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: button.top
                        margins: 10
                    }
                    onClicked: mri_data_provider.buildNavPoints();
                }

                Button {
                    id: button
                    text: "Открыть исследование"
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        margins: 10
                    }
                    onClicked: {
                        open_directory_dialog.open()
                    }
                }
            }

            ColumnLayout {
                id: mid_col
                Layout.fillWidth: true
                Layout.preferredWidth: parent.width * 0.4
                Layout.preferredHeight: parent.height
                spacing: 5

                VtkPlaneViewer {
                    id: plane_viewer_0
                    objectName: "plane_viewer0"
                    orientation: 0
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.45
                }

                Slider {
                    id: slider_0
                    from: 0
                    value: to / 2
                    to: mri_data_provider.slices_0
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.05
                    onMoved: mri_data_provider.setSlice_0(value)
                }

                VtkPlaneViewer {
                    id: plane_viewer_1
                    objectName: "plane_viewer1"
                    orientation: 1
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.45
                }

                Slider {
                    id: slider_1
                    from: 0
                    value: to / 2
                    to: mri_data_provider.slices_1
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.05
                    onMoved: mri_data_provider.setSlice_1(value)
                }
            }

            ColumnLayout {
                id: right_col
                Layout.fillWidth: true
                Layout.preferredWidth: parent.width * 0.4
                Layout.preferredHeight: parent.height
                spacing: 5

                VtkPlaneViewer {
                    id: plane_viewer_2
                    objectName: "plane_viewer2"
                    orientation: 2
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.45
                }

                Slider {
                    id: slider_2
                    from: 0
                    value: to / 2
                    to: mri_data_provider.slices_2
                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.05
                    onMoved: mri_data_provider.setSlice_2(value)
                }

                VtkModelViewer {
                    id: model_viewer
                    objectName: "model_viewer"
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: plane_viewer_1.height
                }

                Rectangle {
                    opacity: 0
                    Layout.preferredWidth: parent.width
                    Layout.preferredHeight: parent.height * 0.05
                }
            }
        }
    }

    FolderDialog {
        id: open_directory_dialog
        visible: false
        title: "Выберите папку с исследованием"
        onAccepted: {
            mri_data_provider.setDirectory(currentFolder)
        }
    }
}
