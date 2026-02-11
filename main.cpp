#pragma execution_character_set("utf-8")
#include <QtWidgets\QApplication>
#include <QTextCodec>
#include <windows.h> 
#include "VoiceTalkWidget.h"
#include "UdpVoiceChat.h"

int main(int argc, char* argv[]) {

    // 设置控制台输入/输出为UTF-8（兼容Qt的UTF-8字符串）
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    QApplication a(argc, argv);
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(codec);  // 本地编码用UTF-8

    QFont globalFont("微软雅黑", 10);
    a.setFont(globalFont);

    UdpVoiceChat  w;
    w.resize(800, 400); 
    w.show();

    return a.exec();
}
