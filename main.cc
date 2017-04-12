
#include <unistd.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

ChatDialog::ChatDialog()
{
	setWindowTitle("P2Papp");

	// Read-only text box where we display messages from everyone.
	// This widget expands both horizontally and vertically.
	textview = new QTextEdit(this);
	textview->setReadOnly(true);

	// Small text-entry box the user can enter messages.
	// This widget normally expands only horizontally,
	// leaving extra vertical space for the textview widget.
	//
	// You might change this into a read/write QTextEdit,
	// so that the user can easily enter multi-line messages.
	textline = new QLineEdit(this);

	// Lay out the widgets to appear in the main window.
	// For Qt widget and layout concepts see:
	// http://doc.qt.nokia.com/4.7-snapshot/widgets-and-layouts.html
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textview);
	layout->addWidget(textline);
	setLayout(layout);

	// Register a callback on the textline's returnPressed signal
	// so that we can send the message entered by the user.
	connect(textline, SIGNAL(returnPressed()),
		this, SLOT(gotReturnPressed()));
}

void ChatDialog::gotReturnPressed()
{
	// QString msg = textline->text();
	emit writeMessage(textline->text());
/*

	// Pack message
	QMap<QString, QVariant> map;
	map.insert("ChatText", textline->text());

	QByteArray body;
	QDataStream out(&body, QIODevice::WriteOnly);
	out << map;

	for (int p = myPortMin; p <= myPortMax; p++) {
		udpSocket.writeDatagram(body, QHostAddress::LocalHost, p);
	}
	//
	qDebug() << "Message sent: " << textline->text();
	textview->append(textline->text());
*/

	// Clear the textline to get ready for the next input message.
	textline->clear();
}

void ChatDialog::gotNewMessage(QString msg, QString user)
{
	textview->append(msg);
}

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four P2Papp instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	udpSocket = new QUdpSocket(this);
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (udpSocket->bind(QHostAddress::LocalHost, p)) {//if (QUdpSocket::bind(p)) {
			qDebug() << "bound to UDP port " << p;
			connect(udpSocket, SIGNAL(readyRead()), this, SLOT(recMessage()));
			//Assign default values
			origin = QString::number((qrand() % 100) + 100);
			seqNum = 0;
			msgLog = new QMap<QString, QVector<QByteArray> *>();
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

void NetSocket::sendMessage(QString msg)
{

		// Pack message
		QMap<QString, QVariant> map;
		map.insert("ChatText", msg);
		map.insert("Origin", origin);
		map.insert("SeqNo", seqNum++); //increment after assigning

		QByteArray body;
		QDataStream out(&body, QIODevice::WriteOnly);
		out << map;

		// Add message to log
		if (!(msgLog->contains(origin))) //New user
			msgLog->insert(origin, new QVector<QByteArray>(0));

		msgLog->value(origin)->append(body);

		// Send out
		for (int p = myPortMin; p <= myPortMax; p++) {
			udpSocket->writeDatagram(body, QHostAddress::LocalHost, p);
		}
		//
		qDebug() << "Message sent[Me|" << seqNum - 1 << "]: " << msg;
		emit messageSent(msg);
}

void NetSocket::recMessage()
{
	QByteArray datagram;
	// Empty out queue till the last message
	while (udpSocket->hasPendingDatagrams()) {
		datagram.resize(udpSocket->pendingDatagramSize());
		udpSocket->readDatagram(datagram.data(), datagram.size());

		// Receive into QMap
		QMap<QString, QVariant> map;
		QDataStream in(&datagram, QIODevice::ReadOnly);
		in >> map;

		//Decode QMap
		if(map.contains("Want")) { //Status message
			QMap<QString, QVariant> want = map.value("Want");

			bool goodStatus = true;
			for (qmap_it iter = want.begin(); iter != want.end(); iter++) {
				QString origin = iter.key();
				quint32 seqNo = iter.value().toUInt();
				if (!msgLog->contains(origin)) {
					qDebug() << "New node discovered! Adding to log";
					msgLog->insert(origin, new QVector<QByteArray>);
					sendStatusMessage(address, port);
					goodStatus = false;
					break;
				} else if ((quint32)msgLog->value(origin)->count() < seqNo) {
					qDebug() << "My messages from " << origin << " are outdated";
					sendStatusMessage(address, port);
					goodStatus = false;
					break;
				} else if ((quint32)msgLog->value(origin)->count() > seqNo) {
					qDebug() << "Status Sender has outdated messages from " << origin;
					QVector<QByteArray> * originMsgs = msgLog->value(origin);
					udpSendToRandomNeighbor(originMsgs->at(seqNo), true);
					goodStatus = false;
					break;
				}
			}

			if (goodStatus) {
				qDebug() << "Status looks good!";
				if (qrand() % 2 == 0) {
					qDebug() << "Rumormongering with random peer";
					int i = qrand() % numNeighbors;
					sendStatusMessage(QHostAddress::LocalHost, neighbors[i]);
				}
				qDebug() << "Rumormongering over";
				printMsgLogCounts();
			}
		}
		else { //Chat message
			QString msg = map.value("ChatText").toString();
			QString user = map.value("Origin").toString();
			quint32 seq = map.value("SeqNo").toInt();
			qDebug() << "Message received[" << user <<"|" << seq << "]: " << msg ;

			if (!(msgLog->contains(user))) //New user
				msgLog->insert(origin, new QVector<QByteArray>(0));

			msgLog->value(user)->append(datagram);
			emit incomingMessage(msg, user);
		}
	} //endWhile
}

void NetSocket::sendStatus()
{
	QMap<QString, QVariant> want;
	for (msglog_it iter = msgLog->begin(); iter != msgLog->end(); iter++) {
		want.insert(iter.key(), QVariant(iter.value()->count()));
	}

	QMap<QString, QVariant> status;
	status.insert("Want", QVariant(want));

	QByteArray body;
	QDataStream out(&body, QIODevice::WriteOnly);
	out << map;
	//Send out
	udpSocket->writeDatagram(body, QHostAddress::LocalHost, p);
	qDebug() << "Status sent";
}


int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	ChatDialog dialog;
	dialog.show();

	// Create a UDP network socket
	NetSocket sock;
	if (!sock.bind())
		exit(1);
	//message UI -> sock
	QObject::connect(&dialog, SIGNAL(writeMessage(QString)), &sock, SLOT(sendMessage(QString)));
	//message sock ->UI
	QObject::connect(&sock, SIGNAL(incomingMessage(QString, QString)), &dialog, SLOT(gotNewMessage(QString, QString)));
	QObject::connect(&sock, SIGNAL(messageSent(QString)), &dialog, SLOT(gotNewMessage(QString)));

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
