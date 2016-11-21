#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QtSerialPort>
#include <vector>

class QHexEdit;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    bool sendCommand(const QByteArray& cmd);

private slots:
    void on_serialConnect_clicked();
    void on_actionOpen_Program_triggered();
    void on_enableLVP_clicked();
    void on_stopLVP_clicked();
    void on_downloadButton_clicked();
    void on_readButton_clicked();
    void on_eraseButton_clicked();
    void on_idButton_clicked();
    void on_clearLogButton_clicked();

private:
    std::vector<QLabel*> serialLabels;
    void querySerialPorts();

    Ui::MainWindow *ui;
    QSerialPort* serialPort;
    QHexEdit* hexEdit;
    QHexEdit* dbgHex;

    unsigned int FlashSizeKB = 128;
    int RowSize = 128;
};

enum {
  CMD_START_PROG = 0x40,
  CMD_END_PROG, //A
  CMD_LATCH_DATA,
  CMD_ERASE_ROW,
  CMD_WRITE_ROW,
  CMD_ERASE_CHIP, //E
  CMD_QUERYINFO,
  CMD_WRITE_CONF, //L
  CMD_OK = 0x20
};

#define RSP_OK 0x61
#define RSP_ERROR 0x62
#define RSP_OUT_OF_SYNC 0x63

#endif // MAINWINDOW_H
