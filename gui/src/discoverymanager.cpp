// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <discoverymanager.h>
#include <exception.h>
#include <settings.h>

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#define PING_MS		500
#define HOSTS_MAX	16
#define DROP_PINGS	3

HostMAC DiscoveryHost::GetHostMAC() const
{
	QByteArray data = QByteArray::fromHex(host_id.toUtf8());
	if(data.size() != 6)
		return HostMAC();
	return HostMAC((uint8_t *)data.constData());
}

static void DiscoveryServiceHostsCallback(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user);
static void DiscoveryServiceHostsManualCallback(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user);

DiscoveryManager::DiscoveryManager(QObject *parent) : QObject(parent)
{
	chiaki_log_init(&log, CHIAKI_LOG_ALL & ~CHIAKI_LOG_VERBOSE, chiaki_log_cb_print, nullptr);

	service_active = false;
	service_active_ipv6 = false;
}

DiscoveryManager::~DiscoveryManager()
{
	if(service_active)
		chiaki_discovery_service_fini(&service);
	if(service_active_ipv6)
		chiaki_discovery_service_fini(&service_ipv6);
	qDeleteAll(manual_services);
}

void DiscoveryManager::SetActive(bool active)
{
	if(service_active == active)
		return;
	service_active = active;

	if(active)
	{
		ChiakiDiscoveryServiceOptions options = {};
		options.ping_ms = PING_MS;
		options.hosts_max = HOSTS_MAX;
		options.host_drop_pings = DROP_PINGS;
		options.cb = DiscoveryServiceHostsCallback;
		options.cb_user = this;

		struct sockaddr_in in_addr = {};
		in_addr.sin_family = AF_INET;
		in_addr.sin_addr.s_addr = 0xffffffff; // 255.255.255.255
		struct sockaddr_storage addr;
		memcpy(&addr, &in_addr, sizeof(in_addr));
		options.send_addr = &addr;
		options.send_addr_size = sizeof(in_addr);
		options.send_host = nullptr;
#ifdef _WIN32
		options.win_broadcast_addrs = nullptr;
		options.win_broadcast_num = 0;
#endif

		ChiakiDiscoveryServiceOptions options_ipv6 = {};
		options_ipv6.ping_ms = PING_MS;
		options_ipv6.hosts_max = HOSTS_MAX;
		options_ipv6.host_drop_pings = DROP_PINGS;
		options_ipv6.cb = DiscoveryServiceHostsCallback;
		options_ipv6.cb_user = this;
		struct sockaddr_in6 in_addr_ipv6 = {};
		in_addr_ipv6.sin6_family = AF_INET6;
		inet_pton(AF_INET6, "FF02::1", &in_addr_ipv6.sin6_addr);
		struct sockaddr_storage addr_ipv6;
		memcpy(&addr_ipv6, &in_addr_ipv6, sizeof(in_addr_ipv6));
		options_ipv6.send_addr = &addr_ipv6;
		options_ipv6.send_addr_size = sizeof(in_addr_ipv6);
		options_ipv6.send_host = nullptr;

#ifdef _WIN32
		QList<int32_t> broadcast_addresses;
		bool status = false;
		uint8_t loc_address[4] = {0};
		uint8_t loc_mask[4] = {0};
		uint8_t loc_broadcast[4] = {0};
		PIP_ADAPTER_INFO pAdapterInfo;
		PIP_ADAPTER_INFO pAdapter = NULL;
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
		DWORD dwRetVal = 0;
		ULONG ulOutBufLen = sizeof (IP_ADAPTER_INFO);
		pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(sizeof (IP_ADAPTER_INFO));
		if (pAdapterInfo == NULL) {
			CHIAKI_LOGE(&log, "Error allocating memory needed to call GetAdaptersinfo\n");
			return;
		}
		if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
			FREE(pAdapterInfo);
			pAdapterInfo = (IP_ADAPTER_INFO *) MALLOC(ulOutBufLen);
			if (pAdapterInfo == NULL) {
				CHIAKI_LOGE(&log, "Error allocating memory needed to call GetAdaptersinfo\n");
				return;
			}
		}

		if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
			pAdapter = pAdapterInfo;
			while (pAdapter) {
				if(pAdapter->Type != IF_TYPE_IEEE80211 && pAdapter->Type != MIB_IF_TYPE_ETHERNET)
				{
					pAdapter = pAdapter->Next;
					continue;
				}
				for(IP_ADDR_STRING *str = &pAdapter->IpAddressList; str != NULL; str = str->Next)
				{
					if(strcmp(str->IpAddress.String, "") == 0)
					{
						continue;
					}
					if(strcmp(str->IpAddress.String, "0.0.0.0") == 0)
					{
						continue;
					}
					inet_pton(AF_INET, str->IpAddress.String, &loc_address);
					inet_pton(AF_INET, str->IpMask.String, &loc_mask);
					for(int i = 0; i < 4; i++)
					{
						loc_broadcast[i] = loc_address[i] | ~loc_mask[i];
					}
					uint32_t final = 0;
					memcpy(&final, &loc_broadcast, 4);
					char address[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &loc_broadcast, address, INET_ADDRSTRLEN);
					if(!broadcast_addresses.contains(final))
						broadcast_addresses.append(final);
					status = true;
				}
				pAdapter = pAdapter->Next;
			}
		} else {
			CHIAKI_LOGE(&log, "GetAdaptersInfo failed with error: %d\n", dwRetVal);

		}
		if (pAdapterInfo)
			FREE(pAdapterInfo);

		if(!status)
		{
			CHIAKI_LOGE(&log, "Couldn't find a valid local address!");
		}
#undef MALLOC
#undef FREE
		options.win_broadcast_addrs = (struct sockaddr_storage *)malloc(broadcast_addresses.size() * sizeof(struct sockaddr_storage));
		if(!options.win_broadcast_addrs)
		{
			CHIAKI_LOGE(&log, "Error allocating memory for broadcast addresses!");
			return;
		}
		for(int i = 0; i < broadcast_addresses.size(); i++)
		{
			struct sockaddr_in in_addr_broadcast = {};
			in_addr_broadcast.sin_family = AF_INET;
			in_addr_broadcast.sin_addr.s_addr = broadcast_addresses[i];
			memcpy(&options.win_broadcast_addrs[i], &in_addr_broadcast, sizeof(in_addr_broadcast));
			options.win_broadcast_num++;
		}
#endif
		ChiakiErrorCode err = chiaki_discovery_service_init(&service, &options, &log);
#ifdef _WIN32
		if(options.win_broadcast_addrs)
			free(options.win_broadcast_addrs);
#endif
		if(err != CHIAKI_ERR_SUCCESS)
		{
			service_active = false;
			CHIAKI_LOGE(&log, "DiscoveryManager failed to init Discovery Service IPV4");
			return;
		}
		err = chiaki_discovery_service_init(&service_ipv6, &options_ipv6, &log);
		if(err != CHIAKI_ERR_SUCCESS)
		{
			service_active_ipv6 = false;
			CHIAKI_LOGE(&log, "DiscoveryManager failed to init Discovery Service IPV6");
		}

		UpdateManualServices();
	}
	else
	{
		chiaki_discovery_service_fini(&service);
		if(service_active_ipv6)
		{
			chiaki_discovery_service_fini(&service_ipv6);
			service_active_ipv6 = false;
		}
		qDeleteAll(manual_services);
		manual_services.clear();

		hosts = {};
		emit HostsUpdated();
	}

}

void DiscoveryManager::SetSettings(Settings *settings)
{
	this->settings = settings;
	connect(settings, &Settings::ManualHostsUpdated, this, &DiscoveryManager::UpdateManualServices);
	connect(settings, &Settings::RegisteredHostsUpdated, this, &DiscoveryManager::UpdateManualServices);
	UpdateManualServices();
}

void DiscoveryManager::SendWakeup(const QString &host, const QByteArray &regist_key, bool ps5)
{
	QByteArray key = regist_key;
	for(size_t i=0; i<key.size(); i++)
	{
		if(!key.at(i))
		{
			key.resize(i);
			break;
		}
	}

	bool ok;
	uint64_t credential = (uint64_t)QString::fromUtf8(key).toULongLong(&ok, 16);
	if(key.size() > 8 || !ok)
	{
		CHIAKI_LOGE(&log, "DiscoveryManager got invalid regist key for wakeup");
		throw Exception("Invalid regist key");
	}
	char *ipv6 = strchr(host.toUtf8().data(), ':');
	ChiakiErrorCode err;
	if(ipv6)
		err = chiaki_discovery_wakeup(&log, service_active_ipv6 ? &service_ipv6.discovery : nullptr, host.toUtf8().constData(), credential, ps5);
	else
		err = chiaki_discovery_wakeup(&log, service_active ? &service.discovery : nullptr, host.toUtf8().constData(), credential, ps5);

	if(err != CHIAKI_ERR_SUCCESS)
		throw Exception(QString("Failed to send Packet: %1").arg(chiaki_error_string(err)));
}

const QList<DiscoveryHost> DiscoveryManager::GetHosts() const
{
	QList<DiscoveryHost> ret = hosts;
	QSet<QString> discovered_hosts;
	for(auto host : std::as_const(ret))
		discovered_hosts.insert(host.host_addr);
	for(auto s : std::as_const(manual_services))
		if(s->discovered && !discovered_hosts.contains(s->discovery_host.host_addr))
			ret.append(s->discovery_host);
	return ret;
}

void DiscoveryManager::DiscoveryServiceHosts(QList<DiscoveryHost> hosts)
{
	this->hosts = std::move(hosts);
	emit HostsUpdated();
}

void DiscoveryManager::UpdateManualServices()
{
	if(!settings || !service_active)
		return;

	QSet<QString> hosts;
	for(auto host : settings->GetManualHosts())
		if(settings->GetRegisteredHostRegistered(host.GetMAC()))
			hosts.insert(host.GetHost());

	const auto keys = manual_services.keys();
	for(auto key : keys)
		if(!hosts.contains(key))
			delete manual_services.take(key);

	for(auto host : std::as_const(hosts))
	{
		if(manual_services.contains(host))
			continue;

		ManualService *s = new ManualService;
		s->manager = this;
		manual_services[host] = s;

		ChiakiDiscoveryServiceOptions options = {};
		options.ping_ms = PING_MS;
		options.ping_initial_ms = PING_MS;
		options.hosts_max = 1;
		options.host_drop_pings = DROP_PINGS;
		options.cb = DiscoveryServiceHostsManualCallback;
		options.cb_user = s;

		QByteArray host_utf8 = host.toUtf8();
		options.send_host = host_utf8.data();
		char *ipv6 = strchr(options.send_host, ':');
		struct sockaddr_storage addr;
		if(ipv6)
		{
			addr.ss_family = AF_INET6;
			options.send_addr = &addr;
			options.send_addr_size = sizeof(struct sockaddr_in6);
		}
		else
		{
			addr.ss_family = AF_INET;
			options.send_addr = &addr;
			options.send_addr_size = sizeof(struct sockaddr_in);
		}
		ChiakiErrorCode err = chiaki_discovery_service_init(&s->service, &options, &log);
		if(err != CHIAKI_ERR_SUCCESS)
		{
			CHIAKI_LOGE(&log, "DiscoveryManager failed to init manual discovery service for host: %s with error %s", qPrintable(host), chiaki_error_string(err));
			manual_services.remove(host);
			continue;
		}
	}
}

class DiscoveryManagerPrivate
{
	public:
		static void DiscoveryServiceHosts(DiscoveryManager *discovery_manager, const QList<DiscoveryHost> &hosts)
		{
			QMetaObject::invokeMethod(discovery_manager, "DiscoveryServiceHosts", Qt::ConnectionType::QueuedConnection, Q_ARG(QList<DiscoveryHost>, hosts));
		}

		static void DiscoveryServiceManualHost(DiscoveryManager *discovery_manager)
		{
			QMetaObject::invokeMethod(discovery_manager, &DiscoveryManager::HostsUpdated);
		}
};

static QList<DiscoveryHost> CreateHostsList(ChiakiDiscoveryHost *hosts, size_t hosts_count)
{
	QList<DiscoveryHost> hosts_list;
	hosts_list.reserve(hosts_count);

	for(size_t i=0; i<hosts_count; i++)
	{
		ChiakiDiscoveryHost *h = hosts + i;
		DiscoveryHost o = {};
		o.ps5 = chiaki_discovery_host_is_ps5(h);
		o.state = h->state;
		o.host_request_port = o.host_request_port;
#define CONVERT_STRING(name) if(h->name) { o.name = QString::fromLocal8Bit(h->name); }
		CHIAKI_DISCOVERY_HOST_STRING_FOREACH(CONVERT_STRING)
#undef CONVERT_STRING
		hosts_list.append(o);
	}

	return hosts_list;
}

static void DiscoveryServiceHostsCallback(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user)
{
	DiscoveryManagerPrivate::DiscoveryServiceHosts(reinterpret_cast<DiscoveryManager *>(user), CreateHostsList(hosts, hosts_count));
}

static void DiscoveryServiceHostsManualCallback(ChiakiDiscoveryHost *hosts, size_t hosts_count, void *user)
{
	ManualService *s = reinterpret_cast<ManualService *>(user);
	s->discovered = hosts_count;
	if(s->discovered)
		s->discovery_host = CreateHostsList(hosts, hosts_count).at(0);
	DiscoveryManagerPrivate::DiscoveryServiceManualHost(s->manager);
}
