#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QObject>
#include <QIcon>

class StyleManager : public QObject {
    Q_OBJECT
   public:
    StyleManager(QObject* parent = 0);

    virtual ~StyleManager() {}

    bool isLightStyle() const { return m_isLightStyle; }
    void setDarkStyle(bool b) { m_isLightStyle = b; }

    QString getResourcePath(const QString& name);
    QIcon getIcon(const QString& name);

    void switchToDark();
    void switchToLight();

   private:
    bool m_isLightStyle;
};

#endif  // STYLEMANAGER_H