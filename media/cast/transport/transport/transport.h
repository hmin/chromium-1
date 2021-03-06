// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_TRANSPORT_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_TRANSPORT_H_

#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"  // PacketSender
#include "media/cast/transport/cast_transport_config.h"
#include "net/udp/udp_server_socket.h"


namespace media {
namespace cast {
namespace transport {

class LocalUdpTransportData;
class LocalPacketSender;

// Helper class for Cast test applications.
class Transport {
 public:
  Transport(scoped_refptr<base::TaskRunner> io_thread_proxy);
  ~Transport();

  // Specifies the ports and IP address to receive packets on.
  // Will start listening immediately.
  void SetLocalReceiver(PacketReceiver* packet_receiver,
                        std::string ip_address,
                        std::string local_ip_address,
                        int port);

  // Specifies the destination port and IP address.
  void SetSendDestination(std::string ip_address, int port);

  PacketSender* packet_sender();

  void StopReceiving();

 private:
  scoped_ptr<net::UDPServerSocket> udp_socket_;
  scoped_refptr<LocalUdpTransportData> local_udp_transport_data_;
  scoped_refptr<LocalPacketSender> packet_sender_;
  scoped_refptr<base::TaskRunner> io_thread_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Transport);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_TRANSPORT_H_
