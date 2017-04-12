#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

typedef QMap<QString, QVariant>::iterator qmap_it;
typedef QMap<QString, QVector<QByteArray>*>::iterator msglog_it;

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed(); //Text is entered
	void gotNewMessage(QString, QString user = "Me");	//Display incoming message

signals:
	void writeMessage(QString); //Signal new message needs to be sent

private:
	QTextEdit *textview;
	QLineEdit *textline;
};

class NetSocket : public QUdpSocket
{
	Q_OBJECT

public:
	NetSocket();

	// Bind this socket to a P2Papp-specific default port.
	bool bind();

public slots:
	void sendMessage(QString); //Wait for meesage from UI
	void recMessage(); //Wait for message from socket
	void sendStatus(); // Update others of status

signals:
	void incomingMessage(QString, QString);
	void messageSent(QString);

private:
	int myPortMin, myPortMax;
	QUdpSocket * udpSocket;
	qint32 seqNum;
	QString origin;
	QMap<QString, QVector<QByteArray> *> *msgLog; // History of messages
};

#endif // P2PAPP_MAIN_HH
