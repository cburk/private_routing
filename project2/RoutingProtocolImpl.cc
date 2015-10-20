#include "RoutingProtocolImpl.h"
#include "global.h"
#include <string.h>

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
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
  num_dif_ports = num_ports;
  my_id = router_id;
  my_protocol_type = protocol_type;

  // Set the ping alarm that goes off every 10 seconds
  char *ping_d = (char *) malloc(5);
  strcpy(ping_d, "ping");
  sys->set_alarm(this, 10000, ping_d);

  // Set any other alarms
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // handle ping alarm
  if(strcmp((char *) data, "ping") == 0){
    for(unsigned short i = 0; i < num_dif_ports; i ++){
      //create the PING packet to send
      pingpong_packet *ret = (pingpong_packet *) malloc(sizeof(struct pingpong_packet));
      packet_header *pp_head = &(ret->head);

      pp_head->packet_type = PING;
      //size is the base 8 bytes + 4 for payload
      pp_head->size = 12;
      pp_head->source_id = my_id;

      ret->head = *pp_head;
      ret->payload = sys->time();
      printf("\n\nsending payload: %i, stored at %p \n\n", ret->payload, &(ret->payload));

      //send it
      sys->send(i, ret, 16);
    }

    // Set the next alarm
    char *ping_d = (char *) malloc(5);
    strcpy(ping_d, "ping");
    sys->set_alarm(this, 10000, ping_d);
  }

  // handle any other alarms

  // Free data malloc'd for message
  free(data);
}

void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
  packet_header *pack_header = (packet_header *) packet;

  switch(pack_header->packet_type)
  {
    case DATA:{
      // If the packet was destined for us, free it as it's now useless
      if ( *((unsigned short *) pack_header->dest_id) == my_id){
        free(packet);
      }
      // Otherwise route it to where it needs to go
      else{
        //TODO: determine correct port to send on
        unsigned short newport = 0;
        sys->send(newport, packet, size);
      }

      break;
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
      ret->payload = ping->payload;

      sys->send(port, ret, 16); //should prob be sizeof pp

      //free received ping message, since we're done with it
      free(ping);

      break;
    }
    case PONG:{
      pingpong_packet *pong = (pingpong_packet *) packet;

      printf("Pong message broken down: %i!!!%i\n", sizeof(struct pingpong_packet), sizeof(struct packet_header));
      printf("type: pong \n");
      printf("reserved ignored\n");
      printf("Size: %i\n", pong->head.size);
      printf("Source: %i\n", pong->head.source_id);
      printf("Dest: %i\n", pong->head.dest_id);
      printf("Payload: %i\n", pong->payload);

      // compute the time it took to reach other end of port
      unsigned int time_dif = sys->time() - pong->payload;

      // update relevant mappings
      id_dist_map[pong->head.source_id] = time_dif;
      id_port_map[pong->head.source_id] = port;
      printf("\n\nReceived payload: %i at loc: %p \n", pong->payload, &(pong->payload));
      printf("Setting cost of %i at time %i\n\n", time_dif, sys->time());

      //free received pong message, since we're done with it
      free(pong);

      break;
    }
    case DV:{
      break;
    }
    case LS:{
      //Not doing this, no-op?
      break;
    }
  }
}

// add more of your own code
