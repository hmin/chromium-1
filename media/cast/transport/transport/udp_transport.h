// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_server_socket.h"

namespace net {
class IOBuffer;
class IPEndPoint;
}  // namespace net

namespace media {
namespace cast {
namespace transport {

// This class implements UDP transport mechanism for Cast.
class UdpTransport : public PacketSender {
 public:
  // Construct a UDP transport.
  // All methods must be called on |io_thread_proxy|.
  // |local_end_point| specifies the address and port to bind and listen
  // to incoming packets.
  // |remote_end_point| specifies the address and port to send packets
  // to. If the value is 0.0.0.0:0 the the end point is set to the source
  // address of the first packet received.
  UdpTransport(const scoped_refptr<base::TaskRunner>& io_thread_proxy,
               const net::IPEndPoint& local_end_point,
               const net::IPEndPoint& remote_end_point);
  virtual ~UdpTransport();

  // Start receiving packets. Packets are submitted to |packet_receiver|.
  void StartReceiving(PacketReceiver* packet_receiver);

  // PacketSender implementations.
  virtual bool SendPackets(const PacketList& packets) OVERRIDE;
  virtual bool SendPacket(const Packet& packet) OVERRIDE;

 private:
  void ReceiveOnePacket();
  void OnReceived(int result);
  void OnSent(const scoped_refptr<net::IOBuffer>& buf, int result);

  scoped_refptr<base::TaskRunner> io_thread_proxy_;
  net::IPEndPoint local_addr_;
  net::IPEndPoint remote_addr_;
  scoped_ptr<net::UDPServerSocket> udp_socket_;
  scoped_refptr<net::IOBuffer> recv_buf_;
  net::IPEndPoint recv_addr_;
  PacketReceiver* packet_receiver_;
  base::WeakPtrFactory<UdpTransport> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UdpTransport);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_UDP_TRANSPORT_H_
