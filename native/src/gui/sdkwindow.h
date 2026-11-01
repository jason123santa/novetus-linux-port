#pragma once
#include <QDialog>

class Config;

// Port of ClientScriptDocumentation: read-only documentation viewer.
class DocsWindow : public QDialog
{
    Q_OBJECT
public:
    explicit DocsWindow(QWidget *parent = nullptr);
};

// Port of SplashTester: shows random/special splashes on demand.
class SplashTesterWindow : public QDialog
{
    Q_OBJECT
public:
    SplashTesterWindow(const Config *user, QWidget *parent = nullptr);
};

// Port of NovetusSDK (the SDK launcher hub): a list of the SDK tools with
// the ability to open each one.
class SdkWindow : public QDialog
{
    Q_OBJECT
public:
    explicit SdkWindow(QWidget *parent = nullptr);

private:
    void openTool(int index);
};
