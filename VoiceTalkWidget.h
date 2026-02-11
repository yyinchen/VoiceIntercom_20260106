#pragma once
#ifndef VOICETALKWIDGET_H
#define VOICETALKWIDGET_H
#include "Config.hpp"

class VoiceTalkWidget : public QWidget
{
    Q_OBJECT

public:
    VoiceTalkWidget(QWidget* parent = nullptr);
    ~VoiceTalkWidget();

private slots:
    // 开始/停止对讲
    void onTalkToggle();
    // 接收局域网音频数据并播放
    void receiveAudioData();

    void onPrintLog(const QString& logContent);
    void onPrintLog(QString logContent, int num);
    void onNotify();

private:
    // 初始化音频格式（16kHz/16bit/单声道，通用兼容）
    void initAudioFormat();
    // 初始化网络（UDP）
    void initNetwork();

    QList<QAudioDeviceInfo> removeDuplicateAudioDevices(const QList<QAudioDeviceInfo>& deviceList);

    // 音频相关
    QAudioFormat m_audioFormat;
    QAudioInput* m_audioInput = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QBuffer* m_inputBuffer = nullptr;   // 采集缓冲区
    QBuffer* m_outputBuffer = nullptr;  // 播放缓冲区
    //QTimer* m_sendTimer = nullptr;      // 音频发送定时器
    QMutex m_inputMutex; // 新增：输入缓冲区锁
    QMutex m_outputMutex;// 新增：输出缓冲区锁
    QMutex m_logMutex;

    // 网络相关
    QUdpSocket* m_udpSocket = nullptr;
    quint16 m_localPort = 8888;         // 本地端口
    QString m_peerIp = "192.168.2.49"; // 对方IP（局域网）
    quint16 m_peerPort = 8888;          // 对方端口

    // UI控件
    QLineEdit* m_peerIpEdit = nullptr;
    QLineEdit* m_peerPortEdit = nullptr;
    QPushButton* m_talkBtn = nullptr;
    QPlainTextEdit* m_logTextEdit = nullptr;
    bool m_isTalking = false;
    bool m_isSend = false;
    bool m_isRecv = false;
    QAudioDeviceInfo m_inputDev;
    QAudioDeviceInfo m_outputDev;
};

#endif // VOICETALKWIDGET_H