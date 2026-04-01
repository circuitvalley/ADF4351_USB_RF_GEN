#include <QApplication>
#include "usbioboard.h"

int main(int argc, char *argv[])
{
    // Enable automatic high-DPI scaling so the entire UI
    // (including absolute-positioned widgets) scales uniformly
    // on monitors with scale factors > 100%.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    USBIOBoard w;
    w.show();

    return a.exec();
}
