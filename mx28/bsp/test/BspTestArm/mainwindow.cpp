#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "bsp.h"
#include "qbsp.h"

#include <syslog.h>

QBsp *g_bsp;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindowClass)
{
    ui->setupUi(this);

    g_bsp = (new Bsp())->m_bsp;
    g_bsp->wait_for_pc_connect(BSP_SERVER_PORT);
}

MainWindow::~MainWindow()
{
    delete ui;
}
