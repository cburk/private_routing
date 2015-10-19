#include "RoutingProtocolImpl.h"
#include "global.h"

struct packet_header {
  ePacketType packet_type;
  char reserved;
  unsigned short size;
  unsigned short source_id;
  unsigned short dest_id;
};

struct pingpong_packet {
  packet_header head;
  unsigned int payload;
};

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  unsigned short num_dif_ports;
  unsigned short my_id;
  eProtocolType my_protocol_type;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  num_dif_ports = num_ports;
  my_id = router_id;
  my_protocol_type = protocol_type;
  // add your own code
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // add your own code
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  packet_header *pack_header = (packet_header *) packet;

  switch(pack_header->packet_type)
  {
    case DATA:{
      // If the packet was destined for us, free it as it's now useless
      if ( *((unsigned short *) pack_header->dest_id) == my_id)
        free(packet);
      // Otherwise route it to where it needs to go
      else{
        //TODO: determine correct port to send on
        unsigned short newport = 0;
        sys->send(newport, packet, size);
      }
    }
    case PING:{
      pingpong_packet *ping = (pingpong_packet *) packet;

      // Just send a pong to the source w/ the same timestamp
      pingpong_packet *ret = (pingpong_packet *) malloc(sizeof(struct pingpong_packet));
      packet_header *pp_head = &(ret->head);

      pp_head->packet_type = PONG;
      //size is the base 8 bytes + 4 for payload
      pp_head->size = 12;
      pp_head->source_id = my_id;
      pp_head->dest_id = ping->head.source_id;

      ret->head = *pp_head;
      ret->payload = sys->time();

      sys->send(port, ret, 12);

      //free received ping message, since we're done with it
      free(ping);
    }
    case PONG:{
      
    }
    case DV:{

    }
    case LS:{
      //Not doing this, no-op?
    }
  }
}

// add more of your own code
