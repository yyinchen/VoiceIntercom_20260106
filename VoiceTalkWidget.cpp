#pragma execution_character_set("utf-8")
#include "VoiceTalkWidget.h"


// Windows平台需额外包含
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

const int MIN_WRITE_SIZE = 2048*2;

VoiceTalkWidget::VoiceTalkWidget(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* _btnLayout = new QHBoxLayout();
    _btnLayout->setContentsMargins(0, 0, 0, 0);  // 去除边距
    _btnLayout->setSpacing(0);                

    m_peerIpEdit = new QLineEdit(m_peerIp, this);
    m_peerIpEdit->setPlaceholderText("输入对方局域网IP");
    m_peerPortEdit = new QLineEdit(QString::number(m_peerPort), this);
    m_peerPortEdit->setPlaceholderText("输入对方端口");
    m_talkBtn = new QPushButton("开始对讲", this);
    connect(m_talkBtn, &QPushButton::clicked, this, &VoiceTalkWidget::onTalkToggle);

    QPushButton* _Btn1 = new QPushButton("发送", this);
    connect(_Btn1, &QPushButton::clicked, [=]() {
            m_isSend = !m_isSend;
            if(m_isSend)
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

    _btnLayout->addWidget(m_peerIpEdit);
    _btnLayout->addWidget(m_peerPortEdit);
    _btnLayout->addWidget(m_talkBtn);
    _btnLayout->addWidget(_Btn1);
    _btnLayout->addWidget(_Btn2);
    _btnLayout->setStretch(0, 4);
    _btnLayout->setStretch(1, 1);
    _btnLayout->setStretch(2, 2);
    _btnLayout->setStretch(1, 1);
    _btnLayout->setStretch(2, 1);

    m_logTextEdit = new QPlainTextEdit(this);
    m_logTextEdit->setMinimumHeight(300);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));

    // 布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(_btnLayout);
    layout->addWidget(m_logTextEdit);
    setLayout(layout);
    setWindowTitle("Qt 局域网语音对讲");
    resize(300, 150);

    // 1. 初始化音频格式
    initAudioFormat();

    // 2. 初始化网络
   //initNetwork();
}

VoiceTalkWidget::~VoiceTalkWidget()
{
    // 停止对讲并释放资源
    m_isTalking = false;
    if (m_audioInput) {
        m_audioInput->stop();
        delete m_audioInput;
    }
    if (m_audioOutput) {
        m_audioOutput->stop();
        delete m_audioOutput;
    }
    //if (m_sendTimer) {
    //    m_sendTimer->stop();
    //    delete m_sendTimer;
    //}
    delete m_inputBuffer;
    delete m_outputBuffer;
    delete m_udpSocket;
}

QList<QAudioDeviceInfo> VoiceTalkWidget::removeDuplicateAudioDevices(const QList<QAudioDeviceInfo>& deviceList)
{
    QList<QAudioDeviceInfo> uniqueList;
    QSet<QString> deviceNames; // 存储已出现的设备名称，快速判重

    for (const QAudioDeviceInfo& dev : deviceList) {
        QString devName = dev.deviceName().trimmed(); // 去除首尾空格，避免因空格导致的“伪重复”
        if (devName.isEmpty()) {
            continue; // 跳过空名称的无效设备
        }

        // 若设备名称未出现过，加入结果列表，并记录名称
        if (!deviceNames.contains(devName)) {
            uniqueList.append(dev);
            deviceNames.insert(devName);
        }
    }

    for (auto& dev : uniqueList)
    {
        onPrintLog("设备名称：" + dev.deviceName());
    }

    return uniqueList;
}

void VoiceTalkWidget::initAudioFormat()
{
#ifdef Q_OS_WIN
    qputenv("QT_AUDIO_BACKEND", "wasapi");
#endif

    // 核心：设置通用音频格式（确保两端兼容）
    m_audioFormat.setSampleRate(44100);    // 16kHz采样率（低延迟）
    m_audioFormat.setChannelCount(1);      // 单声道
    m_audioFormat.setSampleSize(16);       // 16bit位深
    m_audioFormat.setCodec("audio/pcm");   // PCM原始音频（无压缩）
    m_audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    m_audioFormat.setSampleType(QAudioFormat::SignedInt);

    QList<QAudioDeviceInfo> availableInputs = removeDuplicateAudioDevices(QAudioDeviceInfo::availableDevices(QAudio::AudioInput));
    onPrintLog("枚举到的输入设备数：", availableInputs.size());
    if (availableInputs.isEmpty()) 
        m_inputDev = QAudioDeviceInfo::defaultInputDevice();
    else
        m_inputDev = availableInputs[0];
    onPrintLog("绑定输入设备数：" + m_inputDev.deviceName());

    QList<QAudioDeviceInfo> availableOutputs = removeDuplicateAudioDevices(QAudioDeviceInfo::availableDevices(QAudio::AudioOutput));
    onPrintLog("枚举到的输出设备数：", availableOutputs.size());
    if (availableOutputs.isEmpty())
        m_outputDev = QAudioDeviceInfo::defaultInputDevice();
    else
        m_outputDev = availableOutputs[0];
    onPrintLog("绑定输出设备数：" + m_outputDev.deviceName());

    // 验证格式是否支持（适配平板/PC）
    if (!m_inputDev.isFormatSupported(m_audioFormat))
    {
        //m_audioFormat = inputDev.nearestFormat(m_audioFormat);
        m_audioFormat = m_inputDev.preferredFormat(); // 强制使用设备默认格式
        onPrintLog("输入设备不支持格式，使用兼容格式：", m_audioFormat.sampleRate());
    }

    if (!m_outputDev.isFormatSupported(m_audioFormat))
    {
        m_audioFormat = m_outputDev.preferredFormat(); // 强制使用设备默认格式
        onPrintLog("输出设备不支持格式，使用兼容格式：", m_audioFormat.sampleRate());
    }

    // 初始化缓冲区
    m_inputBuffer = new QBuffer(this);
    if (!m_inputBuffer->open(QIODevice::ReadWrite)) { // 必须是ReadWrite模式
        onPrintLog("m_inputBuffer缓冲区打开失败，无法写入音频数据！");
        return;
    }
    m_outputBuffer = new QBuffer(this);
    if (!m_outputBuffer->open(QIODevice::ReadWrite)) { // 必须是ReadWrite模式
        onPrintLog("m_outputBuffer缓冲区打开失败，无法写入音频数据！");
        return;
    }
}

void VoiceTalkWidget::initNetwork()
{
    if (m_udpSocket)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &VoiceTalkWidget::receiveAudioData);
        delete m_udpSocket;
    }
        
    m_udpSocket = new QUdpSocket(this);
    if (!m_udpSocket->bind(QHostAddress::Any, m_localPort)) 
        onPrintLog("绑定本地端口失败：", m_localPort);
    else 
        onPrintLog("已绑定本地端口：", m_localPort);
    

    // 绑定接收数据信号
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &VoiceTalkWidget::receiveAudioData);

    // 初始化发送定时器（每50ms发送一次音频数据，低延迟）
    //m_sendTimer = new QTimer(this);
    //connect(m_sendTimer, &QTimer::timeout, this, &VoiceTalkWidget::sendAudioData);
    //m_sendTimer->setInterval(100);
}

void VoiceTalkWidget::onTalkToggle()
{
    m_isTalking = !m_isTalking;
    if (m_isTalking)
    {
        initNetwork();

        // 开始对讲：启动音频采集和发送
        m_talkBtn->setText("停止对讲");
        m_peerIp = m_peerIpEdit->text();
        m_peerPort = m_peerPortEdit->text().toUInt();

        // 启动音频输入（采集麦克风）
        m_audioInput = new QAudioInput(m_inputDev, m_audioFormat, this);
        m_audioInput->setNotifyInterval(50);
        connect(m_audioInput, &QAudioInput::notify, this, &VoiceTalkWidget::onNotify);
        if (!m_inputBuffer->open(QIODevice::ReadWrite))  // 必须是ReadWrite模式
            onPrintLog("m_inputBuffer缓冲区打开失败");

        //int bufferSize = m_audioFormat.sampleRate() * m_audioFormat.channelCount() * (m_audioFormat.sampleSize() / 8) * 0.1;
       // m_audioInput->setBufferSize(bufferSize);
        m_audioInput->start(m_inputBuffer);

        // 启动音频输出（准备播放）
        m_audioOutput = new QAudioOutput(m_audioFormat, this);
        m_audioOutput->setNotifyInterval(50);
        if (!m_outputBuffer->open(QIODevice::ReadWrite))  // 必须是ReadWrite模式
            onPrintLog("m_outputBuffer缓冲区打开失败");
        //QByteArray preBuffer(2048, 0);
        //m_outputBuffer->write(preBuffer);
        //m_outputBuffer->seek(0); // 仅预填充时seek(0)，后续写入不再用
        //m_audioOutput->setVolume(0.5);
        m_audioOutput->start(m_outputBuffer);

        connect(m_audioOutput, &QAudioOutput::stateChanged, this, [=](QAudio::State state) {
            onPrintLog("播放状态：" , state);
            // 欠载后恢复激活状态
            if (state == QAudio::IdleState && m_audioOutput->error() == QAudio::UnderrunError) {
                m_audioOutput->resume(); // 重启播放
            }
            });

        onPrintLog("开始对讲，目标IP：" + m_peerIp+ " 端口：", m_peerPort);
    }
    else 
    {
        m_talkBtn->setText("开始对讲");
        if (m_audioInput) 
        {
            m_audioInput->stop();
            disconnect(m_audioInput, &QAudioInput::notify, this, &VoiceTalkWidget::onNotify);
            delete m_audioInput;
            m_audioInput = nullptr;
        }
        if (m_audioOutput) 
        {
            m_audioOutput->stop();
            delete m_audioOutput;
            m_audioOutput = nullptr;
        }
        onPrintLog("停止对讲");
    }
}

void VoiceTalkWidget::receiveAudioData()
{
    QByteArray _receiveBuffer;
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());

        QHostAddress senderAddress;
        quint16 senderPort;

        qint64 bytesRead = m_udpSocket->readDatagram(datagram.data(), datagram.size(),
            &senderAddress, &senderPort);

        if (bytesRead > 0 && m_audioOutput) {

            _receiveBuffer.append(datagram);

                // 保持缓冲区大小，避免无限增长
                if (_receiveBuffer.size() > MIN_WRITE_SIZE * 2) {
                    _receiveBuffer = _receiveBuffer.right(MIN_WRITE_SIZE);
                }

                // 当缓冲区有足够数据时开始播放
                if (_receiveBuffer.size() >= MIN_WRITE_SIZE) {
                    m_outputBuffer->write(_receiveBuffer);
                    onPrintLog("\t接收前10字节 " + _receiveBuffer.left(10).toHex());
                    onPrintLog("\t 写入播放数据字节 ", _receiveBuffer.size());
                    _receiveBuffer.clear();
                }
            }
        }
    return;

    if (!m_audioOutput || m_audioOutput->error() != QAudio::NoError)
    {
        onPrintLog("音频写入失败，m_audioOutput->error()： ", m_audioOutput->error());
        return;
    }

    // 1. 读取UDP数据
    while (m_udpSocket->hasPendingDatagrams()) 
    {
        qint64 size = m_udpSocket->pendingDatagramSize();
        if (size <= 0) continue;
        QByteArray data(size, 0);
        m_udpSocket->readDatagram(data.data(), size);
        _receiveBuffer.append(data);
    }

    // 2. 增大累计阈值：从512改为2048字节（适配48kHz采样率）
    const int MIN_WRITE_SIZE = 2048;
    if (_receiveBuffer.size() < MIN_WRITE_SIZE) {
        return; // 数据不够，先不写入
    }

    // 3. 写入播放缓冲区（核心：不seek(0)）
    m_outputMutex.lock();
    // 移到缓冲区末尾，追加写入
    m_outputBuffer->seek(m_outputBuffer->size());
    qint64 writeLen = m_outputBuffer->write(_receiveBuffer);
    // 不调用 seek(0)！！！

    // 4. 验证并清空接收缓冲区
    if (writeLen > 0) 
    {
        onPrintLog("接收字节 ", _receiveBuffer.size());
        onPrintLog("\t接收前10字节 " + _receiveBuffer.left(10).toHex());
        onPrintLog("\t 写入播放数据字节 ", writeLen);
        _receiveBuffer.clear();
    }
    else 
        onPrintLog("播放数据写入失败 ");
    m_outputMutex.unlock();

}

void VoiceTalkWidget::onPrintLog(const QString& logContent)
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

void VoiceTalkWidget::onPrintLog(QString logContent, int num)
{
    logContent +=  QString::number(num);
    onPrintLog(logContent);
}

void VoiceTalkWidget::onNotify()
{
    if (!m_isTalking || !m_isSend)  return;

    m_inputMutex.lock();
    onPrintLog("缓冲区原始大小： ", m_inputBuffer->size());
    QByteArray audioData = m_inputBuffer->readAll();
    //m_inputBuffer->setData(QByteArray()); // 清空缓冲区，避免堆积
    m_inputBuffer->seek(0);
    m_inputMutex.unlock();

    if (audioData.size() < 1 )
    {
        onPrintLog("发送失败：audioData.size()  " , audioData.size());
        onPrintLog("\tQAudioInput状态： ", m_audioInput->state());
    }
    else
    {
        qint64 sendLen = m_udpSocket->writeDatagram(audioData, QHostAddress(m_peerIp), m_peerPort);
        if (sendLen < 0)
            onPrintLog("发送失败：m_udpSocket->errorString()" + m_udpSocket->errorString());
        else
        {
            m_inputBuffer->setData(QByteArray());
            onPrintLog("发送字节 ", sendLen);
            onPrintLog("\t发送前10字节 " + audioData.left(10).toHex());
        }
    }
}
