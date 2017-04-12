
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
	QString newMsg = "[" + user + "]: " + msg;
	textview->append(newMsg);
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
			//Assign neighbors
			if( p == myPortMax || p == myPortMin) {
				numNeighbors = 1;
				neighbors[0] = 1;
			} else {
				numNeighbors = 2;
				neighbors[0] = p - 1;
				neighbors[1] = p + 1;
			}
			connect(udpSocket, SIGNAL(readyRead()), this, SLOT(recMessage()));
			//Assign default values
			origin = QString::number((qrand() % 100) + 100);
			seqNum = 0;
			msgLog = new QMap<QString, QVector<QByteArray> *>();
			// timer = new QTimer(this);
			// aeTimer = new QTimer(this);
			connect(&timer, SIGNAL(timeout()), this, SLOT(timeOut()));
			connect(&aeTimer, SIGNAL(timeout()), this, SLOT(sendStatusRandom()));
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
		sendMsgRandom(body);

		//
		qDebug() << QString("SENT[Me|" + QString::number(seqNum - 1) + "]: " + msg).toStdString().c_str();
		emit messageSent(msg);
}

void sendMsgRandom(QByteArray datagram)
{
	int i = qrand() % numNeighbors;

	sendMsgNbr(datagram, neighbors[i]);
}

void sendMsgNbr (QByteArray body, int p)
{
	udpSocket->writeDatagram(body, QHostAddress::LocalHost, p);

	//Timer
	timer->start(2000);
}

void NetSocket::recMessage()
{
	QByteArray datagram;
	QHostAddress userHost;
	quint16 userPort;
	// Empty out queue till the last message
	while (udpSocket->hasPendingDatagrams()) {
		datagram.resize(udpSocket->pendingDatagramSize());
		udpSocket->readDatagram(datagram.data(), datagram.size(), &userHost, &userPort);

		// Receive into QMap
		QMap<QString, QVariant> map;
		QDataStream in(&datagram, QIODevice::ReadOnly);
		in >> map;

		//Decode QMap
		if(map.contains("Want")) { //Status message
			timer->stop(); //Ack response
			qDebug("REC status message");
			QMap<QString, QVariant> want = map.value("Want");

			for (qmap_it iter = want.begin(); iter != want.end(); iter++) {
				QString origin = iter.key();
				int lastNum = iter.value().toInt();
				if (!msgLog->contains(origin)) {
					qDebug("New Node: %s", origin);
					msgLog->insert(origin, new QVector<QByteArray>);
					sendStatus(port);
					return;
				} else if (msgLog->value(origin)->count() < lastNum) {
					qDebug("Outdate messages for %s", origin);
					sendStatus(port);
					return;
				} else if ((msgLog->value(origin)->count() > lastNum) {
					qDebug("Updating outdated message for %s", origin);
					QVector<QByteArray> * updatedMsg = msgLog->value(origin);
					sendMsgNbr(updatedMsg->at(lastNum), userPort)
					return;
				}
			}

				qDebug("Status is up-to-date");
				sendStatusRandom();
			}
		}
		else { //Chat message
			QString msg = map.value("ChatText").toString();
			QString user = map.value("Origin").toString();
			quint32 seq = map.value("SeqNo").toInt();
			qDebug() << QString("REC [" + user + "|" + QString::number(seq ) + "]: " + msg).toStdString().c_str();

			if (!(msgLog->contains(user))) //New user
				msgLog->insert(origin, new QVector<QByteArray>(0));

			QVector<QByteArray> * userLog = msgLog->value(user);
			// Check age of message
			if (userLog->count() == (int) seq){ // Next message we're expecting
				msgLog->value(user)->append(datagram);
				emit incomingMessage(msg, user);

				sendMsgRandom();
			}
			// Send status as ack
			sendStatus(userPort);
		}
	} //endWhile
}

void NetSocket::sendStatus(int p)
{
	QMap<QString, QVariant> want;
	for (msglog_it iter = msgLog->begin(); iter != msgLog->end(); iter++) {
		want.insert(iter.key(), QVariant(iter.value()->count()));
	}

	QMap<QString, QVariant> status;
	status.insert("Want", QVariant(want));

	QByteArray body;
	QDataStream out(&body, QIODevice::WriteOnly);
	out << want;
	//Send out
	udpSocket->writeDatagram(body, QHostAddress::LocalHost, p);
	qDebug("Status sent to p%d", p)
}

void NetSocket::sendStatusRandom()
{
	int i = qrand() % numNeighbors;

	sendStatus(neighbors[i]);
}

void NetSocket::timeOut()
{
	qDebug("Timeout sending messsage");
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
	QObject::connect(&dialog, SIGNAL(writeMessage(QString)), &sock, SLOT(sendMsgRandom(QString)));
	//message sock ->UI
	QObject::connect(&sock, SIGNAL(incomingMessage(QString, QString)), &dialog, SLOT(gotNewMessage(QString, QString)));
	QObject::connect(&sock, SIGNAL(messageSent(QString)), &dialog, SLOT(gotNewMessage(QString)));

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
