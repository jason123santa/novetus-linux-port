#include <QApplication>
#include <QCommandLineParser>
#include "core/paths.h"
#include "core/config.h"
#include "guishared.h"
#include "extendedwindow.h"
#include "compactwindow.h"
#include "stylishwindow.h"
#include "consolewindow.h"
#include "serverbrowser.h"
#include "settingsdialog.h"
#include "customizationwindow.h"

int main(int argc, char **argv)
{
    // Follow the desktop theme (SystemColors behavior of the original app)
    // when launched outside a full Plasma session.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME")) {
        qputenv("QT_QPA_PLATFORMTHEME", "kde");
    }
    QApplication app(argc, argv);
    Paths::init();

    QCommandLineParser parser;
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    QCommandLineOption consoleOpt("console", "Open the command console window");
    QCommandLineOption browserOpt("browser", "Open the server browser");
    QCommandLineOption settingsOpt("settings", "Open the settings dialog");
    QCommandLineOption styleOpt("launcher-style", "Force a launcher style (extended|compact|stylish)", "style");
    parser.addOptions({consoleOpt, browserOpt, settingsOpt, styleOpt});
    parser.addHelpOption();
    parser.process(app);

    GuiShared::Ui ui = GuiShared::createConfigs();

    if (parser.isSet(consoleOpt)) {
        ConsoleWindow w;
        w.show();
        return app.exec();
    }
    if (parser.isSet(browserOpt)) {
        ServerBrowser w;
        w.exec();
        return 0;
    }
    if (parser.isSet(settingsOpt)) {
        SettingsDialog w;
        w.exec();
        return 0;
    }

    const QString styleArg = parser.value(styleOpt).toLower();
    int style = ui.user->readSettingInt("LauncherStyle");
    if (!styleArg.isEmpty()) {
        style = styleArg == "extended" ? 1 : styleArg == "compact" ? 2 : 3;
    }
    switch (style) {
    case 1: {
        ExtendedWindow w;
        w.show();
        return app.exec();
    }
    case 2: {
        CompactWindow w;
        w.show();
        return app.exec();
    }
    case 0:
    case 3:
    default: {
        StylishWindow w;
        w.show();
        return app.exec();
    }
    }
}
