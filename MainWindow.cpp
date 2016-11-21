#include <QtSerialPort>
#include <QtWidgets>
#include <fstream>
#include <sstream>
#include <cstdint>
#include "qhexedit/qhexedit.h"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "intelhexclass.h"

void EncodeUInt16(uint16_t i, unsigned char o[])
{
    o[0] = i >> 8;
    o[1] = i;
}

uint16_t DecodeUInt16(unsigned char i[])
{
    int16_t ret = i[0];
    ret <<= 8;
    ret += i[1];
    return ret;
}

void EncodeUInt32(uint32_t i, unsigned char o[]) {
  o[0] = i >> 24;
  o[1] = i >> 16;
  o[2] = i >> 8;
  o[3] = i;
}

uint32_t DecodeUInt32(unsigned char i[]) {
  return ((uint32_t)i[0] << 24) + ((uint32_t)i[1] << 16) +
         ((uint32_t)i[2] << 8) + (uint32_t)i[3];
}

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
    serialPort(nullptr)
{
	ui->setupUi(this);
	querySerialPorts();
    dbgHex = new QHexEdit(this);
    dbgHex->setMinimumWidth(500);
    ui->horizontalLayout->addWidget(dbgHex);
    hexEdit = ui->hexEdit;
    hexEdit->setMinimumWidth(500);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::sendCommand(const QByteArray &cmd) {
    if(serialPort) {
        serialPort->write(cmd);
        serialPort->flush();

        bool ret = true;
        auto readData = serialPort->readAll();
        while (!readData.endsWith("a") && serialPort->waitForReadyRead(1000))
            readData.append(serialPort->readAll());
        if(!readData.endsWith("a"))
            ret = false;
        dbgHex->insert(dbgHex->data().length(), readData);
        return ret;
    }
    return false;
}

void MainWindow::on_serialConnect_clicked()
{
    if(!serialPort)
    {
        serialPort = new QSerialPort(ui->serialCombo->currentText(), nullptr);
        serialPort->setBaudRate(115200);
        serialPort->open(QIODevice::ReadWrite);
        querySerialPorts();

        ui->serialConnect->setText("Disconnect");
    }
    else
    {
		//serialPort->flush();
        serialPort->close();
        delete serialPort;
        serialPort = nullptr;
        querySerialPorts();

        ui->serialConnect->setText("Connect");
    }
}

void MainWindow::on_actionOpen_Program_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Program"), QString(), tr("Intel Hex Files (*.hex *.ihx)"));
    std::ifstream ifProg;
    ifProg.open(fileName.toLocal8Bit().data(), std::ifstream::in);
    if(ifProg.good())
    {
        //C:\Users\cheny\OneDrive\Documents\MPLABXProjects\pic18test.X\dist\XC8_18F47K40\production
        intelhex inthex;
        ifProg >> inthex;
        unsigned long sa, ea;
        inthex.startAddress(&sa);
        inthex.endAddress(&ea);
        qDebug() << "Address: " << sa << " - " << ea;

        int ProgSize;
        inthex.begin();
        while(!inthex.endOfData() && inthex.currentAddress() < FlashSizeKB*1024) {
            ProgSize = inthex.currentAddress();
            inthex++;
        }
        ProgSize++;
        qDebug() << "Size: " << ProgSize;

        QByteArray programData;
        for(int i = 0; i < ProgSize; i++) {
            unsigned char d;
            bool exist = inthex.getData(&d, i);
            if(!exist) {
                d = 0xFF;
                qDebug() << i << " does not exist";
            }
            programData.append(d);
        }
        hexEdit->setData(programData);

        QByteArray confData;
        for(int i = 0; i < 12; i++) {
            unsigned char d;
            inthex.getData(&d, 0x300000+i);
            confData.append(d);
        }
        ui->confEdit->setData(confData);
    }
}

void MainWindow::on_enableLVP_clicked()
{
    if(serialPort)
    {
        QByteArray cmd;
        cmd.append(CMD_START_PROG);
        cmd.append(CMD_OK);
        if(!sendCommand(cmd)) return;
    }
}

void MainWindow::on_stopLVP_clicked()
{
    if(serialPort)
    {
        QByteArray cmd;
        cmd.append(CMD_END_PROG);
        cmd.append(CMD_OK);
        if(!sendCommand(cmd)) return;
    }
}

void MainWindow::on_downloadButton_clicked()
{
    if(serialPort)
    {
        dbgHex->setData(QByteArray());

        auto confArray = ui->confEdit->data();
        QByteArray confCommand;
        confCommand.append(CMD_WRITE_CONF);
        confCommand.append(confArray);
        confCommand.append(CMD_OK);
        if(confArray.length() != 12) {
            qDebug() << "Configration wrong";
            return;
        }
        if(!sendCommand(confCommand)) return;

        uint32_t segmentStart = 0;
        auto segmentData = QByteArray();

        auto wholeProgram = hexEdit->data();
        for(int s = 0; s < wholeProgram.length(); s += RowSize) {
            bool haveSomething = false;
            for(int addr = s; addr < min(s + RowSize, wholeProgram.length()); addr++) {
                if((unsigned char)wholeProgram.at(addr) != 0xFF) {
                    if(segmentData.length() == 0)
                        segmentStart = addr;
                    segmentData.append(wholeProgram.at(addr));
                } else {
                    if(segmentData.length() != 0) {
                        haveSomething = true;
                        QByteArray latchDataCmd;
                        latchDataCmd.append(CMD_LATCH_DATA);
                        unsigned char temp[4];
                        EncodeUInt32(segmentStart, temp);
                        latchDataCmd.append(temp[0]);
                        latchDataCmd.append(temp[1]);
                        latchDataCmd.append(temp[2]);
                        latchDataCmd.append(temp[3]);
                        EncodeUInt16(segmentData.length(), temp);
                        latchDataCmd.append(temp[0]);
                        latchDataCmd.append(temp[1]);
                        latchDataCmd.append(segmentData);
                        latchDataCmd.append(CMD_OK);
                        if(!sendCommand(latchDataCmd)) return;

                        segmentData.clear();
                    }
                }
            }

            if(segmentData.length() != 0) {
                haveSomething = true;
                QByteArray latchDataCmd;
                latchDataCmd.append(CMD_LATCH_DATA);
                unsigned char temp[4];
                EncodeUInt32(segmentStart, temp);
                latchDataCmd.append(temp[0]);
                latchDataCmd.append(temp[1]);
                latchDataCmd.append(temp[2]);
                latchDataCmd.append(temp[3]);
                EncodeUInt16(segmentData.length(), temp);
                latchDataCmd.append(temp[0]);
                latchDataCmd.append(temp[1]);
                latchDataCmd.append(segmentData);
                latchDataCmd.append(CMD_OK);
                if(!sendCommand(latchDataCmd)) return;

                segmentData.clear();
            }

            if(haveSomething) {
                QByteArray cmd;
                cmd.append(CMD_ERASE_ROW);
                unsigned char temp[4];
                EncodeUInt32(s, temp);
                cmd.append(temp[0]);
                cmd.append(temp[1]);
                cmd.append(temp[2]);
                cmd.append(temp[3]);
                cmd.append(CMD_OK);
                if(!sendCommand(cmd)) return;

                cmd.clear();
                cmd.append(CMD_WRITE_ROW);
                EncodeUInt32(s, temp);
                cmd.append(temp[0]);
                cmd.append(temp[1]);
                cmd.append(temp[2]);
                cmd.append(temp[3]);
                cmd.append(CMD_OK);
                if(!sendCommand(cmd)) return;
            }
            ui->statusBar->showMessage("Row: " + QString::number(s / RowSize) + " Done");
        }
    }
}

void MainWindow::on_readButton_clicked()
{
    if(serialPort)
    {
        ui->statusBar->showMessage("Unimplemented");
    }
}

void MainWindow::on_eraseButton_clicked()
{
    if(serialPort)
    {
        QByteArray cmd;
        cmd.append(CMD_ERASE_CHIP);
        cmd.append(CMD_OK);
        if(!sendCommand(cmd)) return;
    }
}

void MainWindow::on_idButton_clicked()
{
    if(serialPort)
    {
        QByteArray cmdBuffer;
        cmdBuffer.append(CMD_QUERYINFO);
        cmdBuffer.append(CMD_OK);
        if(!sendCommand(cmdBuffer)) return;
    }
}

void MainWindow::on_clearLogButton_clicked()
{
    dbgHex->setData(QByteArray());
}

void MainWindow::querySerialPorts()
{
    for(auto* p : serialLabels)
        delete p;
	serialLabels.clear();
    ui->serialCombo->clear();

    auto* layout = ui->serialLayout;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString s = QObject::tr("Port: ") + info.portName() + "\n"
                    + QObject::tr("Location: ") + info.systemLocation() + "\n"
                    + QObject::tr("Description: ") + info.description() + "\n"
                    + QObject::tr("Manufacturer: ") + info.manufacturer() + "\n"
                    + QObject::tr("Serial number: ") + info.serialNumber() + "\n"
                    + QObject::tr("Vendor Identifier: ") + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) + "\n"
                    + QObject::tr("Product Identifier: ") + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) + "\n"
                    + QObject::tr("Busy: ") + (info.isBusy() ? QObject::tr("Yes") : QObject::tr("No")) + "\n";

        QLabel *label = new QLabel(s);
        serialLabels.push_back(label);
        layout->addWidget(label);
        ui->serialCombo->addItem(info.portName());
    }
}
