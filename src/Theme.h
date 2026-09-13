#pragma once
#include <QColor>
#include <QIcon>
#include <QObject>
#include <QString>
#include <QStringList>

// Emite themeChanged() sempre que Theme::apply() troca o tema.
class ThemeHub : public QObject
{
    Q_OBJECT
public:
    static ThemeHub &instance();
signals:
    void themeChanged();
};

// Sistema de temas.
//   "nocturne"      - tema original (azul-grafite, cantos arredondados)
//   "classic_dark"  - Classic Things escuro (cantos retos, metal + ambar)
//   "classic_light" - Classic Things claro (fundo branco, cantos retos)
// A folha :/style/base.qss usa tokens @TOKEN@ substituidos por apply().
namespace Theme
{
    QString accent();      // cor de destaque do tema atual
    QString accentDark();  // estado pressionado / hover cheio
    QString accentText();  // texto sobre a cor de destaque
    QString muted();       // texto secundario
    QString line();        // linhas e bordas
    QString token(const QString &name); // qualquer token da paleta atual
    QColor color(const QString &name);   // token como QColor
    int radius();                        // 8 (Nocturne) ou 0 (Classic Things)
    bool isSquare();                     // true nos temas Classic Things
    // Icone monocromatico recolorido com a cor de icone do tema.
    QIcon icon(const QString &resourcePath);

    QStringList names();                       // chaves dos temas disponiveis
    QString displayName(const QString &themeKey);
    QString normalizeKey(const QString &themeKey); // aceita chaves antigas do INI
    QString current();
    void apply(const QString &themeKey = QString());
}
