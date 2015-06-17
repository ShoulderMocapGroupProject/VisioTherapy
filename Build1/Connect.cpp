#include "Connect.h"


Connect::Connect(QWidget *parent) : QPushButton(parent)
{}

void Connect::clicked1()
{
	emit Connectpressed(address);
}

void Connect::editConnect(QString recieve)
{
	address = recieve.toLocal8Bit().constData();
}