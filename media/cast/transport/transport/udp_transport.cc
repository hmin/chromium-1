// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport/udp_transport.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"

namespace media {
namespace cast {
namespace transport {

namespace {
const int kMaxPacketSize = 1500;

bool IsEmpty(const net::IPEndPoint& addr) {
  net::IPAddressNumber empty_addr(addr.address().size());
  return std::equal(empty_addr.begin(),
                    empty_addr.end(),
                    addr.address().begin());
}

bool IsEqual(const net::IPEndPoint& addr1, const net::IPEndPoint& addr2) {
  return addr1.port() == addr2.port() &&
      std::equal(addr1.address().begin(),
                 addr1.address().end(),
                 addr2.address().begin());
}

}  // namespace

UdpTransport::UdpTransport(
    const scoped_refptr<base::TaskRunner>& io_thread_proxy,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point)
    : io_thread_proxy_(io_thread_proxy),
      local_addr_(local_end_point),
      remote_addr_(remote_end_point),
      udp_socket_(new net::UDPServerSocket(NULL, net::NetLog::Source())),
      recv_buf_(new net::IOBuffer(kMaxPacketSize)),
      packet_receiver_(NULL),
      weak_factory_(this) {
}

UdpTransport::~UdpTransport() {
}

void UdpTransport::StartReceiving(PacketReceiver* packet_receiver) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());
  DCHECK(!packet_receiver_);

  packet_receiver_ = packet_receiver;
  udp_socket_->AllowAddressReuse();
  udp_socket_->SetMulticastLoopbackMode(true);
  udp_socket_->Listen(local_addr_);
  ReceiveOnePacket();
}

void UdpTransport::ReceiveOnePacket() {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  int result = udp_socket_->RecvFrom(
      recv_buf_,
      kMaxPacketSize,
      &recv_addr_,
      base::Bind(&UdpTransport::OnReceived, weak_factory_.GetWeakPtr()));
  if (result > 0) {
    OnReceived(result);
  } else if (result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Failed to receive packet: " << result << "."
               << " Stop receiving packets.";
  }
}

void UdpTransport::OnReceived(int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (result < 0) {
    LOG(ERROR) << "Failed to receive packet: " << result << "."
               << " Stop receiving packets.";
    return;
  }

  if (IsEmpty(remote_addr_)) {
    remote_addr_ = recv_addr_;
    VLOG(1) << "First packet received from: "
            << remote_addr_.ToString() << ".";
  } else if (!IsEqual(remote_addr_, recv_addr_)) {
    VLOG(1) << "Received from an unrecognized address: "
            << recv_addr_.ToString() << ".";
    return;
  }

  // TODO(hclam): The interfaces should use net::IOBuffer to eliminate memcpy.
  uint8* data = new uint8[result];
  memcpy(data, recv_buf_->data(), result);
  packet_receiver_->ReceivedPacket(
      data,
      result,
      base::Bind(&PacketReceiver::DeletePacket, data));
  ReceiveOnePacket();
}

bool UdpTransport::SendPackets(const PacketList& packets) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  bool result = true;
  for (size_t i = 0; i < packets.size(); ++i) {
    result |= SendPacket(packets[i]);
  }
  return result;
}

bool UdpTransport::SendPacket(const Packet& packet) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  // TODO(hclam): This interface should take a net::IOBuffer to minimize
  // memcpy.
  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(
      static_cast<int>(packet.size()));
  memcpy(buf->data(), &packet[0], packet.size());
  int ret = udp_socket_->SendTo(
      buf,
      static_cast<int>(packet.size()),
      remote_addr_,
      base::Bind(&UdpTransport::OnSent,
                 weak_factory_.GetWeakPtr(), buf));
  return ret == net::OK;
}

void UdpTransport::OnSent(const scoped_refptr<net::IOBuffer>& buf,
                          int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (result < 0) {
    VLOG(1) << "Failed to send packet: " << result << ".";
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media
