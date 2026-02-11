#pragma execution_character_set("utf-8")
#include "UdpVoiceChat.h"
#include <opencv2/opencv.hpp>

UdpVoiceChat::UdpVoiceChat(QWidget* parent)
    : QMainWindow(parent), m_isSending(false)
{
    setWindowTitle("UDP Voice Chat");

    // 创建界面组件
    m_targetIPEdit = new QLineEdit(this);
    m_targetIPEdit->setText("192.168.2.49");
    m_targeVoicetPortEdit = new QLineEdit(this);
    m_targeVoicetPortEdit->setText("8100");

    m_targetVideoPortEdit = new QLineEdit(this);
    m_targetVideoPortEdit->setText("8200");
    
    m_startButton = new QPushButton("开始通话", this);
    m_stopButton = new QPushButton("停止通话", this);
    m_stopButton->setEnabled(false);
    QPushButton* _Btn1 = new QPushButton("发送", this);
    connect(_Btn1, &QPushButton::clicked, [=]() {
        m_isSend = !m_isSend;
        if (m_isSend)
            onPrintLog("开启发送");
        else
            onPrintLog("关闭发送");
        });
    QPushButton* _Btn2 = new QPushButton("接收", this);
    connect(_Btn2, &QPushButton::clicked, [=]() {
        m_isRecv = !m_isRecv;
        if (m_isRecv)
            onPrintLog("开启接收");
        else
            onPrintLog("关闭接收");
        });

    QPushButton* _openVideo = new QPushButton("打开视频", this);
    connect(_openVideo, &QPushButton::clicked, [=]() {
            int _targetPort = m_targetVideoPortEdit->text().toUShort();
            if (!m_udpVideoSocket->bind(QHostAddress::Any, _targetPort, QUdpSocket::ShareAddress)) {
                onPrintLog("错误  视频绑定端口失败");
            }
        });
    QPushButton* _closeVideo = new QPushButton("关闭视频", this);
    connect(_closeVideo, &QPushButton::clicked, [=]() {
            m_udpVideoSocket->close();
        });
    m_statusLabel = new QLabel("准备就绪", this);

    // 布局
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* ipLayout = new QHBoxLayout();
    ipLayout->addWidget(new QLabel("目标IP:"));
    ipLayout->addWidget(m_targetIPEdit);
    ipLayout->addWidget(new QLabel("语音端口:"));
    ipLayout->addWidget(m_targeVoicetPortEdit);
    ipLayout->addWidget(new QLabel("视频端口:"));
    ipLayout->addWidget(m_targetVideoPortEdit);

    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_stopButton);
    buttonLayout->addWidget(_Btn1);
    buttonLayout->addWidget(_Btn2);
    buttonLayout->addWidget(_openVideo);
    buttonLayout->addWidget(_closeVideo);

    m_logTextEdit = new QPlainTextEdit(this);
    m_logTextEdit->setFixedSize(960, 200);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));

    m_videoLabel = new QLabel(this);
    m_videoLabel->setFixedSize(960, 540);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->addLayout(ipLayout);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_logTextEdit);
    mainLayout->addWidget(m_videoLabel);

    mainLayout->setStretch(0, 1);
    mainLayout->setStretch(1, 1);
    mainLayout->setStretch(2, 1);
    mainLayout->setStretch(3, 2);
    mainLayout->setStretch(4, 5);

    centralWidget->setLayout(mainLayout);

    // 初始化网络和音频
    m_udpSocket = new QUdpSocket(this);
    initAudio();
    m_udpVideoSocket = new QUdpSocket(this);

    // 连接信号和槽
    connect(m_startButton, &QPushButton::clicked, this, &UdpVoiceChat::on_startButton_clicked);
    connect(m_stopButton, &QPushButton::clicked, this, &UdpVoiceChat::on_stopButton_clicked);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &UdpVoiceChat::on_readVoiceData);
    connect(m_udpVideoSocket, &QUdpSocket::readyRead, this, &UdpVoiceChat::on_readVideoData);
}

UdpVoiceChat::~UdpVoiceChat()
{
    stopSending();
}

void UdpVoiceChat::initAudio()
{
    // 设置音频格式
    m_format.setSampleRate(44100);
    m_format.setChannelCount(2);
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);

    // 检查格式支持
    QAudioDeviceInfo inputInfo = QAudioDeviceInfo::defaultInputDevice();
    if (!inputInfo.isFormatSupported(m_format)) {
        m_format = inputInfo.nearestFormat(m_format);
    }

    QAudioDeviceInfo outputInfo = QAudioDeviceInfo::defaultOutputDevice();
    if (!outputInfo.isFormatSupported(m_format)) {
        m_format = outputInfo.nearestFormat(m_format);
    }

    // 创建音频输入和输出对象
    m_audioInput = new QAudioInput(m_format, this);
    m_audioOutput = new QAudioOutput(m_format, this);
    connect(m_audioOutput, &QAudioOutput::stateChanged, this, &UdpVoiceChat::handleStateChanged);
}

void UdpVoiceChat::on_startButton_clicked()
{
    m_targetAddress = QHostAddress(m_targetIPEdit->text());
    m_targetPort = m_targeVoicetPortEdit->text().toUShort();
    if (m_targetAddress.isNull() || m_targetPort == 0) {
        onPrintLog( "错误 请输入有效的IP地址和端口");
        return;
    }

    // 绑定本地端口，用于接收数据
    if (!m_udpSocket->bind(QHostAddress::Any, m_targetPort, QUdpSocket::ShareAddress)) {
        onPrintLog("错误 语音绑定端口失败");
        return;
    }
    
    startSending();

    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_statusLabel->setText("通话中...");
}

void UdpVoiceChat::on_stopButton_clicked()
{
    stopSending();

    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_statusLabel->setText("已停止");
}

void UdpVoiceChat::startSending()
{
    m_isSending = true;

    // 启动音频输入设备，开始采集音频
    m_inputDevice = m_audioInput->start();
    connect(m_inputDevice, &QIODevice::readyRead, [this]() {
        if (m_isSending)
        {
            QByteArray data = m_inputDevice->readAll();
            // 通过UDP发送音频数据
            if (m_isSend)
            {
                qint64 sendLen = m_udpSocket->writeDatagram(data, m_targetAddress, m_targetPort);
                if (sendLen < 0)
                    onPrintLog("发送失败：m_udpSocket->errorString()" + m_udpSocket->errorString());
                else
                {
                    onPrintLog("发送字节 " + QString::number(sendLen) +
                        "\t前10字节 " + data.left(10).toHex());
                }
            }
        }
        });

    // 启动音频输出设备，准备播放
    m_outputDevice = m_audioOutput->start();
}

void UdpVoiceChat::stopSending()
{
    m_isSending = false;

    if (m_inputDevice) {
        m_inputDevice->disconnect();
        m_audioInput->stop();
    }

    if (m_outputDevice) {
        m_audioOutput->stop();
    }

    m_udpSocket->close();
}

void UdpVoiceChat::on_readVoiceData()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        if (m_isRecv)
        {
            onPrintLog("接收字节 "+ QString::number( datagram.size()) +
                "\t前10字节 "+ datagram.left(10).toHex());

            // 将接收到的音频数据写入音频输出设备进行播放
            if (m_outputDevice && (m_audioOutput->state() == QAudio::ActiveState || m_audioOutput->state() == QAudio::IdleState))
            {
                m_outputDevice->write(datagram);
                onPrintLog("播放数据 ");
            }
        }
    }
}

void UdpVoiceChat::on_readVideoData()
{
    while (m_udpVideoSocket->hasPendingDatagrams()) 
    {
        QNetworkDatagram datagram = m_udpVideoSocket->receiveDatagram();

        if (datagram.isValid())
        {
            QByteArray data = datagram.data();

            // 使用OpenCV解码JPEG
            try 
            {
                std::vector<uchar> buffer(data.begin(), data.end());
                cv::Mat frame = cv::imdecode(buffer, cv::IMREAD_COLOR);

                if (!frame.empty()) 
                {

                    // 转换为Qt图像 (BGR -> RGB)
                    //cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

                    QImage qimg(
                        frame.data,
                        frame.cols,
                        frame.rows,
                        frame.step,
                        QImage::Format_RGB888
                    );

                    // 深拷贝，因为frame是临时变量
                    QImage imageCopy = qimg.copy();
                    // 显示图像
                    QPixmap pixmap = QPixmap::fromImage(imageCopy);

                    // 缩放以适应标签大小
                    if (!pixmap.isNull()) {
                        pixmap = pixmap.scaled(
                            m_videoLabel->size(),
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation
                        );
                        m_videoLabel->setPixmap(pixmap);
                    }
                    
                }
            }
            catch (const cv::Exception& e) {
                qWarning() << "解码图像失败:" << e.what();
            }
            catch (...) {
                qWarning() << "未知错误";
            }
        }
    }
}

void UdpVoiceChat::handleStateChanged(QAudio::State newState)
{
    switch (newState) {
    case QAudio::StoppedState:
        if (m_audioOutput->error() != QAudio::NoError) {
            qDebug() << "Audio output error: " << m_audioOutput->error();
        }
        break;
    default:
        break;
    }
}

void UdpVoiceChat::onPrintLog(const QString& logContent)
{
    QMutexLocker locker(&m_logMutex);

    // 拼接日志：时间戳 + 日志级别 + 内容
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString log = QString("[%1] [%2] %3").arg(timeStr).arg("INFO").arg(logContent);

    // 追加日志到文本框（非阻塞，避免UI卡顿）
    QMetaObject::invokeMethod(m_logTextEdit, [this, log]() {
        // 追加文本（换行）
        m_logTextEdit->appendPlainText(log);
        // 自动滚动到底部（始终显示最新日志）
        QTextCursor cursor = m_logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logTextEdit->setTextCursor(cursor);
        }, Qt::QueuedConnection);
}

void UdpVoiceChat::onPrintLog(QString logContent, int num)
{
    logContent += QString::number(num);
    onPrintLog(logContent);
}