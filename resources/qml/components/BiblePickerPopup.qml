pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import KMedia.UI
import ".."
import "../controls"

// The audio Bible (sign language Bible for sign languages): a book, a chapter, then the verses.
Popup {
    id: root

    property string sectionId: ""
    property int book: 0
    property string bookName: ""
    property int chapterCount: 0
    property int chapter: 0
    property int firstVerse: 0
    property int lastVerse: 0
    readonly property var verseList: root.chapter > 0
                                     ? ApplicationController.bible.verses(root.book, root.chapter) : []
    readonly property string pickedReference: root.firstVerse > 0
        ? ApplicationController.bible.reference(root.book, root.chapter, root.firstVerse,
                                                root.lastVerse > 0 ? root.lastVerse : root.firstVerse)
        : ""

    function openFor(targetSectionId) {
        root.sectionId = targetSectionId
        root.book = 0
        root.chapter = 0
        root.firstVerse = 0
        root.lastVerse = 0
        ApplicationController.bible.load()
        root.open()
    }

    // The first click picks a verse, the second one the end of the range, a third starts over.
    function pickVerse(verse) {
        if (root.firstVerse === 0 || root.lastVerse > 0) {
            root.firstVerse = verse
            root.lastVerse = 0
        } else {
            root.lastVerse = verse
            if (root.lastVerse < root.firstVerse) {
                const first = root.lastVerse
                root.lastVerse = root.firstVerse
                root.firstVerse = first
            }
        }
    }

    function inRange(verse) {
        if (root.firstVerse === 0) {
            return false
        }
        const last = root.lastVerse > 0 ? root.lastVerse : root.firstVerse
        return verse >= root.firstVerse && verse <= last
    }

    width: Math.min(720, parent ? parent.width - 24 : 720)
    height: Math.min(600, parent ? parent.height - 48 : 600)
    padding: 14
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay

    Overlay.modal: Rectangle {
        color: Theme.scrimOverlay
    }

    background: StudioCard {
        surfaceColor: Theme.cardElevated
        strokeColor: Theme.borderStrong
        cornerRadius: Theme.radiusLarge
    }

    Connections {
        target: ApplicationController.bible

        function onVersesAdded(ok) {
            if (ok) {
                root.close()
            } else {
                ApplicationController.showToast(qsTr("These verses could not be added"), "warning")
            }
        }
    }

    component NumberButton: StudioButton {
        id: numberButton

        property int number: 0
        property bool chosen: false

        variant: numberButton.chosen ? "primary" : "secondary"
        text: String(numberButton.number)
        leftPadding: 0
        rightPadding: 0
        implicitWidth: 44
        implicitHeight: 34
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            StudioButton {
                visible: root.book > 0
                variant: "ghost"
                iconName: "chevron-left"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: {
                    if (root.chapter > 0) {
                        root.chapter = 0
                        root.firstVerse = 0
                        root.lastVerse = 0
                    } else {
                        root.book = 0
                    }
                }
            }

            StudioIcon {
                visible: root.book === 0
                name: "audio-lines"
                size: 18
                color: Theme.accentPrimary
            }

            Text {
                Layout.fillWidth: true
                text: root.book === 0
                      ? (ApplicationController.bible.signLanguage ? qsTr("Sign Language Bible") : qsTr("Audio Bible"))
                      : (root.chapter === 0 ? root.bookName : root.bookName + " " + root.chapter)
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            StudioButton {
                variant: "ghost"
                iconName: "x"
                leftPadding: 0
                rightPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                onClicked: root.close()
            }
        }

        Text {
            visible: root.book === 0 && ApplicationController.bible.busy
                     && ApplicationController.bible.books.length === 0
            text: qsTr("Loading...")
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            Flow {
                width: parent.width
                spacing: 6

                // Books
                Repeater {
                    model: root.book === 0 ? ApplicationController.bible.books : []

                    delegate: StudioButton {
                        id: bookButton

                        required property var modelData

                        variant: "secondary"
                        text: bookButton.modelData.name
                        implicitWidth: 150
                        implicitHeight: 34
                        onClicked: {
                            root.book = bookButton.modelData.number
                            root.bookName = bookButton.modelData.name
                            root.chapterCount = bookButton.modelData.chapters
                        }
                    }
                }

                // Chapters
                Repeater {
                    model: root.book > 0 && root.chapter === 0 ? root.chapterCount : 0

                    delegate: NumberButton {
                        required property int index

                        number: index + 1
                        onClicked: root.chapter = number
                    }
                }

                // Verses
                Repeater {
                    model: root.chapter > 0 ? root.verseList : []

                    delegate: NumberButton {
                        required property int modelData

                        number: modelData
                        chosen: root.inRange(modelData)
                        onClicked: root.pickVerse(modelData)
                    }
                }
            }
        }

        RowLayout {
            visible: root.chapter > 0
            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: root.verseList.length === 0
                      ? qsTr("This recording has no verse marks: the whole chapter is added")
                      : (root.firstVerse > 0 ? root.pickedReference : qsTr("Pick the first and the last verse"))
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                elide: Text.ElideRight
            }

            StudioButton {
                variant: "primary"
                iconName: "plus"
                text: qsTr("Add")
                enabled: root.firstVerse > 0 || root.verseList.length === 0
                onClicked: ApplicationController.bible.addVerses(
                    root.book, root.chapter, root.firstVerse,
                    root.lastVerse > 0 ? root.lastVerse : root.firstVerse, root.sectionId)
            }
        }
    }
}
