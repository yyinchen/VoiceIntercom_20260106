#pragma once
#ifndef UDP_VOICE_CHAT_H
#define UDP_VOICE_CHAT_H
#include "Config.hpp"

class UdpVoiceChat : public QMainWindow
{
    Q_OBJECT

public:
    UdpVoiceChat(QWidget* parent = nullptr);
    ~UdpVoiceChat();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_readVoiceData();
    void on_readVideoData();
    void handleStateChanged(QAudio::State newState);

    void onPrintLog(const QString& logContent);
    void onPrintLog(QString logContent, int num);

private:
    void initAudio();
    void startSending();
    void stopSending();

private:
    QUdpSocket* m_udpSocket;
    QUdpSocket* m_udpVideoSocket;
    QAudioInput* m_audioInput;
    QAudioOutput* m_audioOutput;
    QIODevice* m_inputDevice;
    QIODevice* m_outputDevice;
    QAudioFormat m_format;

    QPushButton* m_startButton;
    QPushButton* m_stopButton;
    QLineEdit* m_targetIPEdit;
    QLineEdit* m_targeVoicetPortEdit;
    QLineEdit* m_targetVideoPortEdit;
    QLabel* m_statusLabel;
    QPlainTextEdit* m_logTextEdit = nullptr;
    QLabel* m_videoLabel;

    QMutex m_inputMutex; // 新增：输入缓冲区锁
    QMutex m_outputMutex;// 新增：输出缓冲区锁
    QMutex m_logMutex;

    QHostAddress m_targetAddress;
    quint16 m_targetPort;
    bool m_isSending;

    bool m_isSend = false;
    bool m_isRecv = false;
};

#endif // UDP_VOICE_CHAT_H