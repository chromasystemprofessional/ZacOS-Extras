#include "collarextension.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVirtualObject>

namespace {

struct WireItem {
	QString id, label;
	uint flags = 0;
};

QDBusArgument &operator<<(QDBusArgument &arg, const WireItem &item) {
	arg.beginStructure();
	arg << item.id << item.label << item.flags;
	arg.endStructure();
	return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, WireItem &item) {
	arg.beginStructure();
	arg >> item.id >> item.label >> item.flags;
	arg.endStructure();
	return arg;
}

QByteArray iconBytes(const QImage &icon) {
	if (icon.isNull()) {
		return {};
	}
	const QImage image = icon.convertToFormat(QImage::Format_ARGB32).scaled(16, 16);
	QByteArray bytes;
	bytes.reserve(16 * 16 * 4);
	for (int y = 0; y < 16; ++y) {
		const auto *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
		for (int x = 0; x < 16; ++x) {
			bytes += char(qAlpha(row[x]));
			bytes += char(qRed(row[x]));
			bytes += char(qGreen(row[x]));
			bytes += char(qBlue(row[x]));
		}
	}
	return bytes;
}

} // namespace

Q_DECLARE_METATYPE(WireItem)

class CollarExtensionService::Object : public QDBusVirtualObject {
public:
	explicit Object(CollarExtensionService *owner) : m_owner(owner) {}

	QString introspect(const QString &path) const override {
		if (path != Path) {
			return {};
		}
		return QString("<interface name=\"%1\">"
			"<method name=\"GetModules\"><arg direction=\"out\" type=\"aa{sv}\"/></method>"
			"<method name=\"Activate\"><arg name=\"module\" direction=\"in\" type=\"s\"/>"
			"<arg name=\"item\" direction=\"in\" type=\"s\"/></method>"
			"<signal name=\"Changed\"/></interface>").arg(Interface);
	}

	bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override {
		if (message.interface() != Interface) {
			return false;
		}
		if (message.member() == "GetModules") {
			QList<QVariantMap> list;
			for (const CollarModule &m : m_owner->modules ? m_owner->modules() : QList<CollarModule>()) {
				QList<WireItem> menu;
				for (const CollarMenuItem &item : m.menu) {
					menu << WireItem{ item.id, item.label, item.flags };
				}
				QVariantMap map{ { "id", m.id }, { "label", m.label }, { "description", m.description },
					{ "active", m.active }, { "menu", QVariant::fromValue(menu) } };
				const QByteArray icon = iconBytes(m.icon);
				if (!icon.isEmpty()) {
					map.insert("icon", icon);
				}
				list << map;
			}
			bus.send(message.createReply(QVariant::fromValue(list)));
			return true;
		}
		if (message.member() == "Activate") {
			const QString module = message.arguments().value(0).toString();
			const QString item = message.arguments().value(1).toString();
			if (!m_owner->activate) {
				bus.send(message.createReply());
				return true;
			}
			// The reply waits until the work is done, so its error reaches The Collar's alert.
			message.setDelayedReply(true);
			QDBusConnection connection(bus);
			m_owner->activate(module, item, [message, connection](const QString &error) mutable {
				connection.send(error.isEmpty() ? message.createReply()
					: message.createErrorReply("org.zacos9.CollarExtension.Error", error));
			});
			return true;
		}
		bus.send(message.createErrorReply("org.freedesktop.DBus.Error.UnknownMethod", message.member()));
		return true;
	}

private:
	CollarExtensionService *m_owner;
};

QString CollarExtensionService::prefix() {
	const QByteArray prefix = qgetenv("ZACOS9_COLLAR_EXTENSION_PREFIX");
	return prefix.isEmpty() ? QString("org.zacos9.CollarExtension") : QString::fromUtf8(prefix);
}

CollarExtensionService::CollarExtensionService(const QString &name, QObject *parent)
	: QObject(parent), m_service(prefix() + "." + name), m_object(new Object(this)) {
	qDBusRegisterMetaType<WireItem>();
	qDBusRegisterMetaType<QList<WireItem>>();
	qDBusRegisterMetaType<QList<QVariantMap>>();
}

CollarExtensionService::~CollarExtensionService() {
	auto bus = QDBusConnection::sessionBus();
	bus.unregisterObject(Path);
	bus.unregisterService(m_service);
	delete m_object;
}

bool CollarExtensionService::start(QString *error) {
	auto bus = QDBusConnection::sessionBus();
	if (!bus.isConnected()) {
		*error = "There's no session bus to reach The Collar on.";
		return false;
	}
	if (!bus.registerVirtualObject(Path, m_object)) {
		*error = "Couldn't publish the Collar extension object.";
		return false;
	}
	if (!bus.registerService(m_service)) {
		*error = m_service + " is already taken (is another copy running?).";
		bus.unregisterObject(Path);
		return false;
	}
	return true;
}

void CollarExtensionService::changed() {
	QDBusConnection::sessionBus().send(QDBusMessage::createSignal(Path, Interface, "Changed"));
}
