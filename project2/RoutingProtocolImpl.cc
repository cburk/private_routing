#include "RoutingProtocolImpl.h"
#include "global.h"
#include <string.h>
#include <unordered_map>

void send_dv_update(unsigned short from, unsigned short to, unsigned short on_port,
    unordered_map<unsigned short, unsigned short> id_port_map, 
    unordered_map<unsigned short, unsigned int> id_dist_map,
    unordered_map<unsigned short, unsigned short> neighbors_port_map,
    Node *sys);

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

struct dv_entry {
  unsigned short dest_id;
  unsigned short cost;
};

bool map_contains(std::unordered_map<unsigned short, unsigned short> m, unsigned short key){
  if(m.find(key) == m.end())
    return false;
  return true;
}

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

  // Set the freshness check that occurs every second
  char *fresh_d = (char *) malloc(6);
  strcpy(fresh_d, "fresh");
  sys->set_alarm(this, 1000, fresh_d);  

  // Set the alarm that sends dv_updates every 30 seconds
  char *dvup_d = (char *) malloc(5);
  strcpy(dvup_d, "dvup");
  sys->set_alarm(this, 30000, dvup_d);    

  // Set any other alarms
}

void RoutingProtocolImpl::handle_alarm(void *data) {
  // handle freshness check
  if(strcmp((char *) data, "fresh") == 0){
    //printf("freshness check alarm\n");
    for(auto it = id_dist_map.begin(); it != id_dist_map.end(); ++it){
      id_updated_map[it->first] = id_updated_map[it->first] + 1;
      if(id_updated_map[it->first] >= 45)
        id_dist_map[it->first] = INFINITY_COST;
    }

    // Reset the freshness check that occurs every second
    char *fresh_d = (char *) malloc(6);
    strcpy(fresh_d, "fresh");
    sys->set_alarm(this, 1000, fresh_d);
  }

  // send periodic dv update
  if(strcmp((char *) data, "dvup") == 0){
    //printf("periodic dv update alarm\n");
    // Send all neighbors an update
    for(auto it = neighbors_port_map.begin(); it != neighbors_port_map.end(); ++it){
      send_dv_update(my_id, it->first, neighbors_port_map[it->first],
        id_port_map, id_dist_map, neighbors_port_map, sys);
    }
    // Reset the alarm that sends dv_updates every 30 seconds
    char *dvup_d = (char *) malloc(5);
    strcpy(dvup_d, "dvup");
    sys->set_alarm(this, 30000, dvup_d);    
  } 

  // handle ping alarm
  if(strcmp((char *) data, "ping") == 0){
    //printf("periodic ping alarm\n");
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
      unsigned short destination = pack_header->dest_id;

      // If the packet was destined for us, free it as it's now useless
      if (destination == my_id){
        free(packet);
      }
      // Otherwise route it to where it needs to go
      else{
        // If we know where to send it:
        if(map_contains(id_port_map, destination))
          sys->send(id_port_map[destination], packet, size);
        // Else flood it?  TODO: determine if flooding is correct behavior here
        else
          for(int i = 0; i < num_dif_ports; i++)
            if(i != port)
              sys->send(i, packet, size);
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

      /* Debugging, easily print a pong message */
      /*
      printf("Pong message broken down: %i!!!%i\n", sizeof(struct pingpong_packet), sizeof(struct packet_header));
      printf("type: pong \n");
      printf("reserved ignored\n");
      printf("Size: %i\n", pong->head.size);
      printf("Source: %i\n", pong->head.source_id);
      printf("Dest: %i\n", pong->head.dest_id);
      printf("Payload: %i\n", pong->payload);
      */

      // compute the time it took to reach other end of port
      unsigned int time_dif = sys->time() - pong->payload;
      unsigned short sender = pong->head.source_id;

      /* update relevant mappings */
      id_updated_map[sender] = 0;
      id_dist_map[sender] = time_dif;
      id_port_map[sender] = port;
      neighbors_port_map[sender] = port;

      // If neighbor's cost changes, change cost (and possibly route) of ALL paths
      // through that port
      //TODO: code to update all routes through a port/neighbor

      //TODO: If any costs changed at all, should trigger a dv update flood

      //free received pong message, since we're done with it
      free(packet);

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

/**
 * Sends a distance vector update message from the router w/ id from to
 * the router w/ id to, using poison reverse.  on_port is the port to send
 * the new packet on.
 *
 * Returns the number of entries in the body
 */
void send_dv_update(unsigned short from, unsigned short to, unsigned short on_port,
    unordered_map<unsigned short, unsigned short> id_port_map, 
    unordered_map<unsigned short, unsigned int> id_dist_map,
    unordered_map<unsigned short, unsigned short> neighbors_port_map,
    Node *sys){

  //Determine size of message to send
  int size = sizeof(struct packet_header); //Size must include header size
  for (auto it = id_dist_map.begin(); it != id_dist_map.end(); ++it)
    if (id_dist_map[it->first] != INFINITY_COST)
      size += sizeof(struct dv_entry);

  packet_header *pack = (packet_header *) malloc(size);
  pack->packet_type = DV;
  pack->size = size;
  pack->source_id = from;
  pack->dest_id = to;

  pack ++; //Move pointer to end of header
  dv_entry *cur_dv = (dv_entry *) pack; //Put first dv entry at end of header

  for (auto it = id_dist_map.begin(); it != id_dist_map.end(); ++it){
    // Only add entries whose costs are < infinity (or dest if < inf originally)
    if (id_dist_map[it->first] != INFINITY_COST){

      cur_dv->dest_id = it->first;

      // If our shortest path first routes through to, cost = inf (poison reverse)
      // In other words, if to is our neighbor and (we route to to and this iterations id through the same port)
      if(map_contains(neighbors_port_map, to) && (neighbors_port_map[to] == id_port_map[it->first]) ){
        cur_dv->cost = INFINITY_COST;
      }
      // Otherwise, fill in entry as usual
      else{
        cur_dv->cost = id_dist_map[it->first];
      }
      // Advance cur_dv to space where next entry should start
      cur_dv += sizeof(struct dv_entry);
    }
  }

  //set pack back to what it was at beginning
  pack --;

  /* For debugging, if you wanna see what an update packet being sent looks like */
  /*
  printf("Sending out dv update that looks like: \n");
  printf("Header info: \n");
  
  printf("Body info: \n");
  dv_entry *entry = (dv_entry *)(pack + 1);
  for(int i = sizeof(struct packet_header); i < size; i += sizeof(struct dv_entry)){
    if(entry->cost == INFINITY_COST)
      printf("Update Entry %i: ID: %i, Cost: INFINITY\n", i, entry->dest_id);
    else
      printf("Update Entry %i: ID: %i, Cost: %i\n", i, entry->dest_id, entry->cost);
    entry ++;
  }
  */

  sys->send(on_port, pack, size);
}

