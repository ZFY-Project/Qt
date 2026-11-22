#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("MaraDrow");
    setWindowIcon(QIcon(""));
}

MainWindow::~MainWindow()
{
    delete ui;
}
