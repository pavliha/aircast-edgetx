/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QHash>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "simulatorinterface.h"

extern "C" SimulatorFactory* registerSimu();

struct Control {
  int type;
  int index;
};

static const QHash<QString, Control> controls = {
    {"lh", {SimulatorInterface::INPUT_SRC_STICK, 0}},
    {"lv", {SimulatorInterface::INPUT_SRC_STICK, 1}},
    {"rv", {SimulatorInterface::INPUT_SRC_STICK, 2}},
    {"rh", {SimulatorInterface::INPUT_SRC_STICK, 3}},
    {"s1", {SimulatorInterface::INPUT_SRC_ANALOG, 4}},
    {"6pos", {SimulatorInterface::INPUT_SRC_ANALOG, 5}},
    {"s2", {SimulatorInterface::INPUT_SRC_ANALOG, 6}},
    {"ls", {SimulatorInterface::INPUT_SRC_ANALOG, 7}},
    {"rs", {SimulatorInterface::INPUT_SRC_ANALOG, 8}},
    {"sa", {SimulatorInterface::INPUT_SRC_SWITCH, 0}},
    {"sb", {SimulatorInterface::INPUT_SRC_SWITCH, 1}},
    {"sc", {SimulatorInterface::INPUT_SRC_SWITCH, 2}},
    {"sd", {SimulatorInterface::INPUT_SRC_SWITCH, 3}},
    {"se", {SimulatorInterface::INPUT_SRC_SWITCH, 4}},
    {"sf", {SimulatorInterface::INPUT_SRC_SWITCH, 5}},
    {"sg", {SimulatorInterface::INPUT_SRC_SWITCH, 6}},
    {"sh", {SimulatorInterface::INPUT_SRC_SWITCH, 7}},
};

static const QHash<QString, int> keys = {
    {"menu", 0},  {"exit", 1},  {"enter", 2}, {"pageup", 3},
    {"pagedn", 4}, {"up", 5},    {"down", 6},  {"left", 7},
    {"right", 8}, {"plus", 9},  {"minus", 10}, {"model", 11},
    {"tele", 12}, {"sys", 13},  {"shift", 14}, {"bind", 15},
};

static const QHash<QString, int> switchPositions = {
    {"up", -1}, {"mid", 0}, {"down", 1}};

static constexpr int SIX_POS_STEP = 2048 / 5;
static constexpr int LCD_WIDTH = 480;
static constexpr int LCD_HEIGHT = 272;
static constexpr int CHANNELS = 24;

struct Radio {
  SimulatorInterface* sim;
  quint8 port;
  QTcpSocket* serial = nullptr;
  QVector<qint32> outputs = QVector<qint32>(CHANNELS, 0);
  QByteArray sentLine;
  QByteArray heardLine;
  QByteArray sending;
  QByteArray hearing;
  QImage screen;
};

static QByteArray lastLine(QByteArray& pending, const QByteArray& more, QByteArray& last)
{
  pending += more;
  qsizetype end;
  while ((end = pending.indexOf('\n')) >= 0) {
    last = pending.left(end).trimmed();
    pending.remove(0, end + 1);
  }
  if (pending.size() > 4096) pending.clear();
  return last;
}

static QByteArray command(Radio& radio, const QList<QByteArray>& words)
{
  const QByteArray verb = words.value(0);
  const QString name = QString::fromLatin1(words.value(1)).toLower();
  if (verb == "set" && words.size() == 3 && controls.contains(name)) {
    const Control control = controls[name];
    const QByteArray value = words[2];
    const int raw = control.type == SimulatorInterface::INPUT_SRC_SWITCH &&
                            switchPositions.contains(QString::fromLatin1(value))
                        ? switchPositions[QString::fromLatin1(value)]
                    : name == "6pos" ? (value.toInt() - 1) * SIX_POS_STEP
                                     : value.toInt();
    radio.sim->setInputValue(control.type, control.index, raw);
    return "ok";
  }
  if (verb == "press" && keys.contains(name)) {
    const int key = keys[name];
    const int ms = words.size() > 2 ? words[2].toInt() : 150;
    radio.sim->setKey(key, true);
    QTimer::singleShot(ms, [&radio, key] { radio.sim->setKey(key, false); });
    return "ok";
  }
  if (verb == "channels") {
    QByteArray line = "channels";
    for (qint32 value : radio.outputs) line += ' ' + QByteArray::number(value);
    return line;
  }
  if (verb == "sent") return "sent " + radio.sentLine;
  if (verb == "heard") return "heard " + radio.heardLine;
  if (verb == "lcd" && words.size() == 2) {
    return radio.screen.save(QString::fromLatin1(words[1]))
               ? QByteArray("ok")
               : QByteArray("err cannot write ") + words[1];
  }
  return "err unknown command";
}

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);
  QCommandLineParser args;
  args.addHelpOption();
  args.addOptions({
      {"sd", "SD card directory", "dir"},
      {"settings", "Radio settings directory (defaults to the SD card)", "dir"},
      {"serial-port", "TCP port that carries the radio's serial line", "port", "5770"},
      {"control-port", "TCP port that takes control commands", "port", "5771"},
      {"aux", "Simulated AUX serial port the line uses (0 = AUX1)", "n", "0"},
  });
  args.process(app);

  Radio radio{registerSimu()->create(), static_cast<quint8>(args.value("aux").toUInt())};
  const QString sd = args.value("sd");
  radio.sim->setSdPath(sd, args.isSet("settings") ? args.value("settings") : sd);
  radio.sim->init();

  QObject::connect(radio.sim, &SimulatorInterface::channelOutValueChange, &app,
                   [&radio](quint8 index, qint32 value, qint32) {
                     if (index < CHANNELS) radio.outputs[index] = value;
                   });
  QObject::connect(radio.sim, &SimulatorInterface::auxSerialSendData, &app,
                   [&radio](quint8 port, const QByteArray& data) {
                     if (port != radio.port) return;
                     lastLine(radio.sending, data, radio.sentLine);
                     if (radio.serial) radio.serial->write(data);
                   });
  QObject::connect(radio.sim, &SimulatorInterface::lcdChange, &app, [&radio](bool) {
    radio.screen =
        QImage(radio.sim->getLcd(), LCD_WIDTH, LCD_HEIGHT, QImage::Format_RGB16).copy();
    radio.sim->lcdFlushed();
  });
  QObject::connect(radio.sim, &SimulatorInterface::runtimeError, &app,
                   [](const QString& error) { qWarning("firmware: %s", qPrintable(error)); });

  QTcpServer serial;
  QObject::connect(&serial, &QTcpServer::newConnection, [&] {
    QTcpSocket* client = serial.nextPendingConnection();
    if (radio.serial) radio.serial->deleteLater();
    radio.serial = client;
    QObject::connect(client, &QTcpSocket::readyRead, [&radio, client] {
      const QByteArray data = client->readAll();
      lastLine(radio.hearing, data, radio.heardLine);
      radio.sim->receiveAuxSerialData(radio.port, data);
    });
    QObject::connect(client, &QTcpSocket::disconnected, [&radio, client] {
      if (radio.serial == client) radio.serial = nullptr;
      client->deleteLater();
    });
  });
  serial.listen(QHostAddress::Any, args.value("serial-port").toUShort());

  QTcpServer control;
  QObject::connect(&control, &QTcpServer::newConnection, [&] {
    QTcpSocket* client = control.nextPendingConnection();
    QObject::connect(client, &QTcpSocket::readyRead, [&radio, client] {
      while (client->canReadLine()) {
        const QList<QByteArray> words = client->readLine().simplified().split(' ');
        client->write(command(radio, words) + '\n');
      }
    });
    QObject::connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
  });
  control.listen(QHostAddress::Any, args.value("control-port").toUShort());

  radio.sim->start(nullptr, false);
  qInfo("virtual TX16S: serial on %d, control on %d, SD %s", serial.serverPort(),
        control.serverPort(), qPrintable(sd));
  return app.exec();
}
