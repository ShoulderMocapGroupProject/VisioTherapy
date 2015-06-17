#include "build1.h"
#include "QTOgreWindow.h"
#include "ui_build1.h"


Build1::Build1(QWidget *parent)
	: QMainWindow(parent)
{
	ui->setupUi(this);
	QTOgreWindow* ogreWindow = new QTOgreWindow();
	QWidget* renderingContainer = QWidget::createWindowContainer(ogreWindow);
	renderingContainer->setMinimumSize(200, 200);
	renderingContainer->setFocusPolicy(Qt::TabFocus);
	ui->verticalLayout->addWidget(renderingContainer);


	connect(ui->Btn_Animate, SIGNAL(clicked()), ogreWindow, SLOT(animatestart()));
	connect(ui->Btn_Stop, SIGNAL(clicked()), ogreWindow, SLOT(animatestop()));
	connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), ogreWindow, SLOT(animatelength(int)));
	connect(ui->Txt_Connect, SIGNAL(textEdited(const QString)), ui->Btn_Connect, SLOT(editConnect(QString)));
	connect(ui->Btn_Connect, SIGNAL(clicked()), ui->Btn_Connect, SLOT(clicked1()));
	connect(ui->Btn_Connect, SIGNAL(Connectpressed(std::string)), ogreWindow, SLOT(viconconnect(std::string)));
	connect(ui->Btn_Disconnect, SIGNAL(clicked()), ogreWindow, SLOT(vicondisconnect()));
	connect(this, SIGNAL(meshnamesend(std::string)), ogreWindow, SLOT(createmesh(std::string)));
	connect(this, SIGNAL(animationnamesend(std::string)), ogreWindow, SLOT(setanimation(std::string)));
	connect(this, SIGNAL(amdsend(std::string, std::string)), ogreWindow, SLOT(loadcsv(std::string, std::string)));
	connect(ogreWindow, SIGNAL(amdcsverror()), this, SLOT(csverror()));
	connect(ogreWindow, SIGNAL(mesherror()), this, SLOT(mesherror()));
	connect(ogreWindow, SIGNAL(animationerror()), this, SLOT(animationerror()));
	connect(ui->Btn_Calibrate, SIGNAL(clicked()), ogreWindow, SLOT(enablecalibrate()));
	connect(ui->Btn_Calibrate, SIGNAL(clicked()), this, SLOT(calibratebtnswaptext()));
	connect(this, SIGNAL(closeanimation()), ogreWindow, SLOT(closedata()));
	connect(ogreWindow, SIGNAL(dataloaderror()), this, SLOT(dataerror()));
	connect(ui->checkBox, SIGNAL(stateChanged(int)), ogreWindow, SLOT(skeletonmove(int)));
	calibrate = false;
}

Build1::~Build1()
{

}

void Build1::on_actionOpen_triggered()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
		tr("Text Files (*.txt);;C++ Files (*.cpp *.h)"));

	if (!fileName.isEmpty()) {
		QFile file(fileName);
		if (!file.open(QIODevice::ReadOnly)) {
			QMessageBox::critical(this, tr("Error"), tr("Could not open file"));
			return;
		}
		QTextStream in(&file);
		std::string meshname = (in.readLine()).toLocal8Bit().constData();
		std::string animationname = (in.readLine()).toLocal8Bit().constData();
		std::string amd = (in.readLine()).toLocal8Bit().constData();
		std::string fps = (in.readLine()).toLocal8Bit().constData();
		emit meshnamesend(meshname);
		emit animationnamesend(animationname);
		emit amdsend(amd,fps);
		file.close();
	}
}


void Build1::on_actionClose_triggered()
{
	emit closeanimation();
}

//void Build1::on_actionClose_Animation_triggered()
//{
//	emit closeanimation();
//}

void Build1::csverror()
{
	QMessageBox::critical(this, tr("Error"), tr("Animation data csv not found!"));
}

void Build1::mesherror()
{
	QMessageBox::critical(this, tr("Error"), tr("Mesh not found!"));
}

void Build1::animationerror()
{
	QMessageBox::critical(this, tr("Error"), tr("Animation not found"));
}

void Build1::dataerror()
{
	QMessageBox::critical(this, tr("Error"), tr("Data already loaded"));
}

void Build1::calibratebtnswaptext()
{
	if (calibrate == false)
	{
		calibrate = true;
		ui->Btn_Calibrate->setText("Stop Calibrate");
	}	
	else 
	{
		ui->Btn_Calibrate->setText("Calibrate");
		calibrate = false;
	}
}