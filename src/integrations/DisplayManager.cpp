#include "integrations/DisplayManager.h"

#include <QGuiApplication>
#include <QVariantMap>

namespace kmedia {

DisplayManager::DisplayManager(QObject* parent) : QObject(parent)
{
    if (qGuiApp != nullptr) {
        connect(qGuiApp, &QGuiApplication::screenAdded, this, &DisplayManager::onScreenAdded);
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, &DisplayManager::onScreenRemoved);
        connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this,
                [this](QScreen*) { refreshScreens(); });
    }
    refreshScreens();
}

int DisplayManager::screenCount() const
{
    return static_cast<int>(m_screens.size());
}

bool DisplayManager::hasSecondaryDisplay() const
{
    return m_hasSecondaryDisplay;
}

int DisplayManager::manualScreenIndex() const
{
    return m_manualScreenIndex;
}

void DisplayManager::setManualScreenIndex(int index)
{
    if (m_manualScreenIndex == index) {
        return;
    }
    m_manualScreenIndex = index;
    recomputeTarget();
    emit displaysChanged();
}

int DisplayManager::targetScreenIndex() const
{
    return m_targetScreenIndex;
}

QString DisplayManager::targetScreenName() const
{
    if (m_targetScreenIndex < 0 || m_targetScreenIndex >= static_cast<int>(m_screens.size())) {
        return {};
    }
    return m_screens[static_cast<size_t>(m_targetScreenIndex)].name;
}

QRect DisplayManager::targetGeometry() const
{
    if (m_targetScreenIndex < 0 || m_targetScreenIndex >= static_cast<int>(m_screens.size())) {
        return {0, 0, 1280, 720};
    }
    return m_screens[static_cast<size_t>(m_targetScreenIndex)].geometry;
}

bool DisplayManager::isPresentationWindowVisible() const
{
    return m_presentationVisible;
}

void DisplayManager::setPresentationWindowVisible(bool visible)
{
    m_manualVisibilityOverride = true;
    if (m_presentationVisible == visible) {
        return;
    }
    m_presentationVisible = visible;
    emit visibilityChanged();
}

QVariantList DisplayManager::screensVariant() const
{
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_screens.size()));
    for (int i = 0; i < static_cast<int>(m_screens.size()); ++i) {
        const auto& s = m_screens[static_cast<size_t>(i)];
        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("name"), s.name);
        map.insert(QStringLiteral("x"), s.geometry.x());
        map.insert(QStringLiteral("y"), s.geometry.y());
        map.insert(QStringLiteral("width"), s.geometry.width());
        map.insert(QStringLiteral("height"), s.geometry.height());
        map.insert(QStringLiteral("isPrimary"), s.isPrimary);
        list.append(map);
    }
    return list;
}

int DisplayManager::selectTargetScreenIndex(const std::vector<DisplayDescriptor>& screens,
                                            int manualOverrideIndex)
{
    const int count = static_cast<int>(screens.size());
    if (count <= 0) {
        return -1;
    }

    if (manualOverrideIndex >= 0 && manualOverrideIndex < count) {
        return manualOverrideIndex;
    }

    for (int i = 0; i < count; ++i) {
        if (!screens[static_cast<size_t>(i)].isPrimary) {
            return i;
        }
    }
    return 0;
}

void DisplayManager::refreshScreens()
{
    m_screens.clear();
    if (qGuiApp != nullptr) {
        QScreen* primary = qGuiApp->primaryScreen();
        for (QScreen* screen : QGuiApplication::screens()) {
            if (screen == nullptr) {
                continue;
            }
            connectScreenGeometry(screen);
            DisplayDescriptor desc;
            desc.name = screen->name();
            desc.geometry = screen->geometry();
            desc.isPrimary = (screen == primary);
            m_screens.push_back(std::move(desc));
        }
    }

    recomputeTarget();
    emit displaysChanged();
}

void DisplayManager::resetAutoSelection()
{
    m_manualVisibilityOverride = false;
    setManualScreenIndex(-1);
}

void DisplayManager::onScreenAdded(QScreen* screen)
{
    connectScreenGeometry(screen);
    refreshScreens();
}

void DisplayManager::onScreenRemoved(QScreen* /*screen*/)
{
    refreshScreens();
}

void DisplayManager::connectScreenGeometry(QScreen* screen)
{
    if (screen == nullptr) {
        return;
    }
    // A unique connection needs a member function; with a lambda Qt refuses to connect at all.
    connect(screen, &QScreen::geometryChanged, this, &DisplayManager::refreshScreens,
            Qt::UniqueConnection);
}

void DisplayManager::recomputeTarget()
{
    bool secondaryPresent = false;
    for (const auto& s : m_screens) {
        if (!s.isPrimary) {
            secondaryPresent = true;
            break;
        }
    }
    m_hasSecondaryDisplay = secondaryPresent;
    m_targetScreenIndex = selectTargetScreenIndex(m_screens, m_manualScreenIndex);

    if (!m_manualVisibilityOverride) {
        const bool shouldShow = m_hasSecondaryDisplay || m_manualScreenIndex >= 0;
        if (m_presentationVisible != shouldShow) {
            m_presentationVisible = shouldShow;
            emit visibilityChanged();
        }
    }
}

} // namespace kmedia
