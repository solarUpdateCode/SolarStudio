#include <juce_gui_extra/juce_gui_extra.h>
#include "UI/MainComponent.h"

class SolarStudioApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override       { return "SolarStudio"; }
    const juce::String getApplicationVersion() override    { return "2.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String&) override {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

    class MainWindow : public juce::DocumentWindow {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name, solar::SolarPalette::obsidianBg, DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setIcon(solar::SolarLookAndFeel::getLogoImage(256));
            setContentOwned(new solar::MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(1080, 680, 3840, 2160);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(SolarStudioApplication)
