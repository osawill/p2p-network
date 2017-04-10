#ifndef P2PAPP_MAIN_HH
#define P2PAPP_MAIN_HH

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QUdpSocket>

class ChatDialog : public QDialog
{
	Q_OBJECT

public:
	ChatDialog();

public slots:
	void gotReturnPressed(); //Text is entered
	void gotNewMessage(Qstring msg);	//Display incoming message
	
signals:
	void writeMessage(QString msg); //Signal new message needs to be sent

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
	void sendMessage(Qstring msg); //Wait for meesage from UI
	void recMessage(); //Wait for message from socket
	
signals:
	void incomingMessage(QString msg);

private:
	int myPortMin, myPortMax;
	QUdpSocket udpSocket;
};

#endif // P2PAPP_MAIN_HH
