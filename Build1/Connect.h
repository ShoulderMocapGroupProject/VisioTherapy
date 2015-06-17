#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

class Connect : public QPushButton
{
	Q_OBJECT
public:
	Connect(QWidget *parent = 0);
	std::string address;
	//~Connect();

signals:
	void Connectpressed(std::string address);
public slots:
	void editConnect(QString receive);
	void clicked1();

};