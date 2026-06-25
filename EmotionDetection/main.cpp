#include "EmotionDetection.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/EmotionDetection/app.png"));
    EmotionDetection window;
    window.show();
    return app.exec();
}
