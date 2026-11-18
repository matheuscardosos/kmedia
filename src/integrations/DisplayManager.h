#pragma once

#include <QObject>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QVariantList>

#include <vector>

namespace kmedia {

struct DisplayDescriptor
{
    QString name;
    QRect geometry;
    bool isPrimary{false};
};

class DisplayManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int screenCount READ screenCount NOTIFY displaysChanged FINAL)
    Q_PROPERTY(bool hasSecondaryDisplay READ hasSecondaryDisplay NOTIFY displaysChanged FINAL)
    Q_PROPERTY(int manualScreenIndex READ manualScreenIndex WRITE setManualScreenIndex NOTIFY
                   displaysChanged FINAL)
    Q_PROPERTY(int targetScreenIndex READ targetScreenIndex NOTIFY displaysChanged FINAL)
    Q_PROPERTY(QString targetScreenName READ targetScreenName NOTIFY displaysChanged FINAL)
    Q_PROPERTY(QRect targetGeometry READ targetGeometry NOTIFY displaysChanged FINAL)
    Q_PROPERTY(bool presentationWindowVisible READ isPresentationWindowVisible WRITE
                   setPresentationWindowVisible NOTIFY visibilityChanged FINAL)
    Q_PROPERTY(QVariantList screens READ screensVariant NOTIFY displaysChanged FINAL)

public:
    explicit DisplayManager(QObject* parent = nullptr);

    Q_DISABLE_COPY_MOVE(DisplayManager)

    [[nodiscard]] int screenCount() const;
    [[nodiscard]] bool hasSecondaryDisplay() const;
    [[nodiscard]] int manualScreenIndex() const;
    void setManualScreenIndex(int index);
    [[nodiscard]] int targetScreenIndex() const;
    [[nodiscard]] QString targetScreenName() const;
    [[nodiscard]] QRect targetGeometry() const;
    [[nodiscard]] bool isPresentationWindowVisible() const;
    void setPresentationWindowVisible(bool visible);
    [[nodiscard]] QVariantList screensVariant() const;

    [[nodiscard]] static int selectTargetScreenIndex(const std::vector<DisplayDescriptor>& screens,
                                                     int manualOverrideIndex);

public slots:
    void refreshScreens();
    void resetAutoSelection();

signals:
    void displaysChanged();
    void visibilityChanged();

private slots:
    void onScreenAdded(QScreen* screen);
    void onScreenRemoved(QScreen* screen);

private:
    void connectScreenGeometry(QScreen* screen);
    void recomputeTarget();

    std::vector<DisplayDescriptor> m_screens;
    int m_manualScreenIndex{-1};
    int m_targetScreenIndex{-1};
    bool m_hasSecondaryDisplay{false};
    bool m_presentationVisible{false};
    bool m_manualVisibilityOverride{false};
};

} // namespace kmedia
