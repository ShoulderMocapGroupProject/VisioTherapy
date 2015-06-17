#ifndef BUILD1_H
#define BUILD1_H

#include <QtWidgets/QMainWindow>
#include "ui_build1.h"
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

class Build1 : public QMainWindow
{
	Q_OBJECT

public:
	Build1(QWidget *parent = 0);
	~Build1();
	bool calibrate;
	
signals:
	void meshnamesend(std::string meshname);  //signal for sending name of mesh
	void animationnamesend(std::string animationname);  //signal for sending name of animation
	void amdsend(std::string amk, std::string fps);  //signal for sending file containing marker data for animation
	void closeanimation();

private slots:
	void on_actionClose_triggered();
	void on_actionOpen_triggered();
	//void on_actionClose_Animation_triggered();
	void csverror();
	void mesherror();
	void animationerror();
	void calibratebtnswaptext();
	void dataerror();

private:
	Ui::Build1Class *ui;

};

#endif // BUILD1_H
