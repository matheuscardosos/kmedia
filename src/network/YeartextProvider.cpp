#include "network/YeartextProvider.h"

#include "network/CatalogClient.h"

namespace kmedia {

YeartextProvider::YeartextProvider(CatalogClient* client, QObject* parent)
    : QObject(parent), m_client(client)
{
}

QString YeartextProvider::text() const
{
    return m_text;
}

void YeartextProvider::setText(const QString& text)
{
    if (m_text != text) {
        m_text = text;
        emit textChanged();
    }
}

void YeartextProvider::setLanguages(const QStringList& languages, bool signLanguage, int year)
{
    QStringList ordered;
    if (!signLanguage) {
        for (const QString& language : languages) {
            if (!language.isEmpty() && !ordered.contains(language)) {
                ordered.append(language);
            }
        }
        if (!ordered.isEmpty() && !ordered.contains(QStringLiteral("E"))) {
            ordered.append(QStringLiteral("E"));
        }
    }
    if (ordered == m_languages && year == m_year) {
        return;
    }
    m_languages = ordered;
    m_year = year;
    ++m_request;
    if (m_languages.isEmpty() || m_client == nullptr) {
        setText({});
        return;
    }
    tryLanguage(0);
}

void YeartextProvider::tryLanguage(int index)
{
    if (index >= m_languages.size()) {
        setText({});
        return;
    }
    const quint64 request = m_request;
    m_client->requestYeartext(m_languages.at(index), m_year, this,
                              [this, request, index](bool ok, const QString& text) {
                                  if (request != m_request) {
                                      return;
                                  }
                                  if (ok) {
                                      setText(text);
                                  } else {
                                      tryLanguage(index + 1);
                                  }
                              });
}

} // namespace kmedia
