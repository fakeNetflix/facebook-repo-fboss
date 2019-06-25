// Copyright 2004-present Facebook. All Rights Reserved.

#include "fboss/agent/hw/bcm/BcmWarmBootState.h"

#include "fboss/agent/Constants.h"
#include "fboss/agent/hw/bcm/BcmEgress.h"
#include "fboss/agent/hw/bcm/BcmHost.h"
#include "fboss/agent/hw/bcm/BcmMultiPathNextHop.h"
#include "fboss/agent/hw/bcm/BcmSwitch.h"
#include "fboss/agent/state/RouteNextHopEntry.h"

namespace facebook {
namespace fboss {

template <>
folly::dynamic BcmWarmBootState::toFollyDynamic(
    const BcmHostKey& key,
    const std::shared_ptr<BcmHost>& host) const;

template <>
folly::dynamic BcmWarmBootState::toFollyDynamic(
    const BcmMultiPathNextHopKey& key,
    const std::shared_ptr<BcmMultiPathNextHop>& multiPathNextHop) const;

template <>
folly::dynamic BcmWarmBootState::egressToFollyDynamic(
    const BcmEgress* egress) const;

template <>
folly::dynamic BcmWarmBootState::egressToFollyDynamic(
    const BcmEcmpEgress* egress) const;

folly::dynamic BcmWarmBootState::hostTableToFollyDynamic() const {
  folly::dynamic hostsJson = folly::dynamic::array;
  for (const auto& hostTableEntry : *hw_->getHostTable()) {
    auto host = hostTableEntry.second.lock();
    hostsJson.push_back(toFollyDynamic(hostTableEntry.first, host));
  }

  // previously, ECMP next hops were maintained as a part of BcmHostTable, even
  // though, they did not really go in BCM ASIC's host table. this was reflected
  // in warmboot state file generated by FBOSS for BCMSwitch. Retaining backward
  // compatibility, by doing "multi-Path nexthop" toFollyDynamic here.
  folly::dynamic ecmpHostsJson = folly::dynamic::array;
  auto& ecmpHosts = hw_->getMultiPathNextHopTable()->getNextHops();
  for (const auto& vrfNhopsAndHost : ecmpHosts) {
    auto ecmpHost = vrfNhopsAndHost.second.lock();
    ecmpHostsJson.push_back(
        toFollyDynamic(vrfNhopsAndHost.first, ecmpHost));
  }

  folly::dynamic hostTable = folly::dynamic::object;
  hostTable[kHosts] = std::move(hostsJson);
  hostTable[kEcmpHosts] = std::move(ecmpHostsJson);
  return hostTable;
}

template <>
folly::dynamic BcmWarmBootState::toFollyDynamic(
    const BcmHostKey& hostKey,
    const std::shared_ptr<BcmHost>& bcmHost) const {
  folly::dynamic host = folly::dynamic::object;
  host[kVrf] = hostKey.getVrf();
  host[kIp] = hostKey.addr().str();
  if (hostKey.intfID().hasValue()) {
    host[kIntf] = static_cast<uint32_t>(hostKey.intfID().value());
  }
  host[kPort] = 0;
  auto egressPort = bcmHost->getEgressPortDescriptor();
  if (egressPort) {
    switch (egressPort->type()) {
      case BcmPortDescriptor::PortType::AGGREGATE:
        // TODO: support warmboot for trunks
        break;
      case BcmPortDescriptor::PortType::PHYSICAL:
        host[kPort] = egressPort->toFollyDynamic();
        break;
    }
  }
  host[kEgressId] = bcmHost->getEgressId();
  auto* egress = bcmHost->getEgress();
  if (egress) {
    // owned egress, BcmHost entry is not host route entry.
    host[kEgress] = egressToFollyDynamic(egress);
  }
  return host;
}

template <>
folly::dynamic BcmWarmBootState::toFollyDynamic(
    const BcmMultiPathNextHopKey& key,
    const std::shared_ptr<BcmMultiPathNextHop>& multiPathNextHop) const {
  folly::dynamic ecmpHost = folly::dynamic::object;
  ecmpHost[kVrf] = key.first;
  folly::dynamic nhops = folly::dynamic::array;
  for (const auto& nhop : key.second) {
    nhops.push_back(nhop.toFollyDynamic());
  }
  ecmpHost[kNextHops] = std::move(nhops);
  ecmpHost[kEgressId] = multiPathNextHop->getEgressId();
  ecmpHost[kEcmpEgressId] = multiPathNextHop->getEcmpEgressId();
  auto ecmpEgress = multiPathNextHop->getEgress();
  if (ecmpEgress) {
    ecmpHost[kEcmpEgress] = egressToFollyDynamic(ecmpEgress);
  }
  return ecmpHost;
}

template <>
folly::dynamic BcmWarmBootState::egressToFollyDynamic(
    const BcmEgress* egress) const {
  CHECK(egress);
  folly::dynamic egressDynamic = folly::dynamic::object;
  egressDynamic[kEgressId] = egress->getID();
  egressDynamic[kMac] = egress->getMac().toString();
  egressDynamic[kIntfId] = egress->getIntfId();
  return egressDynamic;
}

template <>
folly::dynamic BcmWarmBootState::egressToFollyDynamic(
    const BcmEcmpEgress* ecmpEgress) const {
  CHECK(ecmpEgress);
  folly::dynamic ecmpEgressDynamic = folly::dynamic::object;
  ecmpEgressDynamic[kEgressId] = ecmpEgress->getID();
  folly::dynamic paths = folly::dynamic::array;
  for (const auto& path : ecmpEgress->paths()) {
    paths.push_back(path);
  }
  ecmpEgressDynamic[kPaths] = std::move(paths);
  return ecmpEgressDynamic;
}

} // namespace fboss
} // namespace facebook