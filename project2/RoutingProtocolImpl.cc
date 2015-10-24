#include "RoutingProtocolImpl.h"
#include "global.h"
#include <string.h>
#include <math.h>
#include <set>
#include <arpa/inet.h>
#include <unordered_map>

/* Sends a dv_update message from router "from" to router "to" on port "on_port" */
void send_dv_update(unsigned short from, unsigned short to, unsigned short on_port,
    unordered_map<unsigned short, unsigned short> id_port_map, 
    unordered_map<unsigned short, unsigned int> id_dist_map,
    unordered_map<unsigned short, unsigned short> neighbors_port_map,
    Node *sys);

struct packet_header {
  unsigned short packet_type; //includes reserved, has to be &'d w/ 0xff00 to get actual type
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

void print_map(unordered_map<unsigned short, unsigned short> mymap){
  printf("\n{");
  for ( auto it = mymap.begin(); it != mymap.end(); ++it ){
    printf(" [ %i, %i ] ", it->first, it->second);
  }
  printf("}\n");
}
void print_map(unordered_map<unsigned short, unsigned int> mymap){
  printf("\n{");
  for ( auto it = mymap.begin(); it != mymap.end(); ++it ){
    printf(" [ %i, %i ] ", it->first, it->second);
  }
  printf("}\n");
}

/* Changes the length of all paths through port by change, does not
 * change the dist for except_for, for example, this could be the neighbor and
 * it's distance could have already been updated
 */
void RoutingProtocolImpl::update_all_through(unsigned short port, int change, unsigned short except_for){
  for (auto it = id_port_map.begin(); it != id_port_map.end(); ++it) {
    if (port == it->second and it->first != except_for){
      if (change == INFINITY_COST){
        id_dist_map[it->first] = INFINITY_COST;
      }
      else
        id_dist_map[it->first] = id_dist_map[it->first] + change;
    }
  }
}

bool map_contains(std::unordered_map<unsigned short, unsigned short> m, unsigned short key){
  if(m.find(key) == m.end())
    return false;
  return true;
}

bool map_contains(std::unordered_map<unsigned short, unsigned int> m, unsigned short key){
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
  sys->set_alarm(this, 0, ping_d);

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
    bool changed = false;
    //printf("freshness check alarm\n");
    for(auto it = id_updated_map.begin(); it != id_updated_map.end(); ++it){
      id_updated_map[it->first] = id_updated_map[it->first] + 1;
      if(id_updated_map[it->first] >= 45){
        //Only set cost to INFINITY if our shortest path is directly through this
        if(id_port_map[it->first] == neighbors_port_map[it->first] && id_dist_map[it->first] != INFINITY_COST){
          id_dist_map[it->first] = INFINITY_COST;
          changed = true;
          //printf("\n\n!!!TIMEOUT!!! %i setting cost to %i to INF!!!\n\n\n", my_id, it->first);
        }
        // If this unresponsive node is our neighbor, set all paths through it to inf cost
        if(map_contains(neighbors_port_map, it->first)){
          //printf("Updating all of %i's paths through the port to %i (which is %i)\n", my_id, it->first, neighbors_port_map[it->first]);
          update_all_through(neighbors_port_map[it->first], INFINITY_COST, it->first);
        }
      }
    }

    // If an entry changed as a result of timeout, flood dv updates
    if(changed){
      for(auto it = neighbors_port_map.begin(); it != neighbors_port_map.end(); ++it){
        //printf("b/c of timeout, sending update to %i thru %i\n", it->first, neighbors_port_map[it->first]);
        send_dv_update(my_id, it->first, neighbors_port_map[it->first]);
      }
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
      //send_dv_update(my_id, it->first, neighbors_port_map[it->first],
      //  id_port_map, id_dist_map, neighbors_port_map, sys);
      send_dv_update(my_id, it->first, neighbors_port_map[it->first]);
    }
    // Reset the alarm that sends dv_updates every 30 seconds
    char *dvup_d = (char *) malloc(5);
    strcpy(dvup_d, "dvup");
    sys->set_alarm(this, 30000, dvup_d);
  } 

  // handle ping alarm
  if(strcmp((char *) data, "ping") == 0){
    //printf("periodic ping alarm\n");
    //for(unsigned short i = 0; i < num_dif_ports; i ++){
    for(unsigned short i = 0; i < num_dif_ports; i ++){
      //create the PING packet to send
      pingpong_packet *ret = (pingpong_packet *) malloc(sizeof(struct pingpong_packet));
      packet_header *pp_head = &(ret->head);

      pp_head->packet_type = PING;

      //size is the base 8 bytes + 4 for payload
      pp_head->size = htons(16);
      pp_head->source_id = htons(my_id);
      pp_head->dest_id = htons(i);

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

    reverse_header(pack_header);
    if(port == 0xffff)
      pack_header->packet_type = (ePacketType) ntohs(pack_header->packet_type) & 0xff00;

  switch(pack_header->packet_type)
  {
    case DATA:{
      unsigned short destination = pack_header->dest_id;
      //printf("\n\nData to %i from %i (size %i)!!\n\n", destination, pack_header->source_id, pack_header->size);

      // If the packet was destined for us, free it as it's now useless
      if (destination == my_id){
        free(packet);
      }
      // Otherwise route it to where it needs to go
      else{
        // If we know where to send it:
        if(map_contains(id_port_map, destination))
          sys->send(id_port_map[destination], packet, size);
        // Else ignore it?
      }

      break;
    }
    case PING:{
      //printf("pingin\n");

      pingpong_packet *ping = (pingpong_packet *) packet;

      // Just send a pong to the source w/ the same timestamp
      pingpong_packet *ret = (pingpong_packet *) malloc(sizeof(struct pingpong_packet));
      packet_header *pp_head = &(ret->head);

      pp_head->packet_type = PONG;
      pp_head->size = htons(16);
      pp_head->source_id = htons(my_id);
      pp_head->dest_id = htons(ping->head.source_id);

      ret->head = *pp_head;
      ret->payload = ping->payload;

      sys->send(port, ret, 16); //should prob be sizeof pp

      //free received ping message, since we're done with it
      free(ping);

      break;
    }
    case PONG:{
      pingpong_packet *pong = (pingpong_packet *) packet;
      int changed = 0; // Keeps track of if any paths have changed, to know whether to dv update or not
      int change = 0;

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
      
      /* ONLY CHANGE IF COST < WHAT WE'VE GOT! otherwise overwrites shorter routes through other nodes*/
      /* Possible concern: what if link is severed on other end?  Addressed by freshness check, will set to inf if necessary */
      if(!map_contains(id_dist_map, sender) or time_dif < id_dist_map[sender]){
        //printf("Changed b/c either doesn't contain?: %i or map entry %i != t_d %i\n", !map_contains(id_dist_map, sender), id_dist_map[sender], time_dif);
        changed = 1;
        change = time_dif - id_dist_map[sender];
        id_dist_map[sender] = time_dif;
        id_port_map[sender] = port;
      }
      neighbors_port_map[sender] = port;

      // If neighbor's cost has changed, we need to flood DV updates and change cost
      // of ALL paths through that port
      if(changed){
        update_all_through(port, change, sender);

        for(auto it = neighbors_port_map.begin(); it != neighbors_port_map.end(); ++it){
          send_dv_update(my_id, it->first, neighbors_port_map[it->first]);
        }
      }

      //free received pong message, since we're done with it
      free(packet);

      break;
    }
    case DV:{
      //given: size and port
      packet_header *pack = (packet_header *)packet;
      unsigned int sender = pack->source_id;
      unsigned int dest;

      int changed = 0;

      dv_entry *entry = (dv_entry *)(pack + 1);

      //printf("Map @ %i before: \n", my_id);
      //print_map(id_dist_map);
      
      // seen is a set of ids we saw distances for in this dv up.  If we have a path
      // routing through port (i.e. going through sender) that isn't seen, we need
      // to set the new dist to infinity (as it was inf for sender, so they ommitted it)  
      std::set<unsigned short> seen;

      // Go through and update everyone else's distance
      for(int i = sizeof(struct packet_header); i < size; i += sizeof(struct dv_entry)){
        dest = ntohs(entry->dest_id);
        entry->cost = ntohs(entry->cost);

        if(dest != my_id) {
          seen.insert(dest);
          
          // If the path through sender is shorter (1) or we've never
          // seen dest before (2)
          // OR our old shortest path to dest is through sender and cost has 
          // changed at all (3), update
          bool prop_one = (map_contains(id_port_map, dest) &&
            (id_dist_map[dest] > id_dist_map[sender] + entry->cost && entry->cost != INFINITY_COST)
          );
          bool prop_two = (!map_contains(id_port_map, dest));
          bool prop_three = (map_contains(id_port_map, dest) && id_port_map[dest] == port && id_dist_map[dest] != id_dist_map[sender] + entry->cost);
          if(prop_one || prop_two || prop_three) {
            changed = 1;
            id_dist_map[dest] = id_dist_map[sender] + entry->cost;
            id_port_map[dest] = port;
          }

        }
        entry ++;
      }

      for(auto it = id_port_map.begin(); it != id_port_map.end(); ++it){
        unsigned short dest = it->first;
        //If our shortest path is through sender but sender didn't send us a dv update
        //for it, its cost is infinity
        if(dest != sender && seen.find(dest) == seen.end() && id_port_map[dest] == port){
          if(id_dist_map[dest] != INFINITY_COST){
            id_dist_map[dest] = INFINITY_COST;
            changed = true;
          }
        }
      }

      // If some entry has changed, we need to flood DV updates
      if(changed)
        for(auto it = neighbors_port_map.begin(); it != neighbors_port_map.end(); ++it){
          send_dv_update(my_id, it->first, neighbors_port_map[it->first]);
        }

      //printf("Map @ %i after: \n", my_id);
      //print_map(id_dist_map);

      free(packet);
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
void RoutingProtocolImpl::send_dv_update(unsigned short from, unsigned short to, unsigned short on_port){
    //unordered_map<unsigned short, unsigned short> id_port_map, 
    //unordered_map<unsigned short, unsigned int> id_dist_map,
    //unordered_map<unsigned short, unsigned short> neighbors_port_map,
    //Node *sys){

  //printf("Here in s_d_v, should be sending dv up from %i to %i\n", from, to);

  //Determine size of message to send
  int size = sizeof(struct packet_header); //Size must include header size
  for (auto it = id_dist_map.begin(); it != id_dist_map.end(); ++it)
    if (id_dist_map[it->first] != INFINITY_COST)
      size += sizeof(struct dv_entry);

  packet_header *pack = (packet_header *) malloc(size);
  pack->packet_type = DV;
  pack->size = htons(size);
  pack->source_id = htons(from);
  pack->dest_id = htons(to);

  pack ++; //Move pointer to end of header
  dv_entry *cur_dv = (dv_entry *) pack; //Put first dv entry at end of header

  for (auto it = id_dist_map.begin(); it != id_dist_map.end(); ++it){
    // Only add entries whose costs are < infinity (or dest if < inf originally)
    if (id_dist_map[it->first] != INFINITY_COST){

      cur_dv->dest_id = htons(it->first);

      // If our shortest path first routes through to, cost = inf (poison reverse)
      // In other words, if to is our neighbor and (we route to to and this iterations id through the same port)
      if(map_contains(neighbors_port_map, to) && (neighbors_port_map[to] == id_port_map[it->first]) ){
        cur_dv->cost = INFINITY_COST;
      }
      // Otherwise, fill in entry as usual
      else{
        cur_dv->cost = htons(id_dist_map[it->first]);
      }
  
      // Advance cur_dv to space where next entry should start
      cur_dv ++;
    }
  }

  //set pack back to what it was at beginning
  pack --;

  /* For debugging, if you wanna see what an update packet being sent looks like */
  /*
  {
  printf("Sending out dv update that looks like: \n");
  printf("Header info: \n");
  
  printf("Body info: \n");
  dv_entry *entry = (dv_entry *)(pack + 1);
  for(int i = sizeof(struct packet_header); i <= size; i += sizeof(struct dv_entry)){
    if(entry->cost == INFINITY_COST)
      printf("Update Entry %i: ID: %i, Cost: INFINITY\n", i, entry->dest_id);
    else
      printf("Update Entry %i: ID: %i, Cost: %i\n", i, entry->dest_id, entry->cost);
    entry ++;
  }
  }
  */

  sys->send(on_port, pack, size);
}

/* Reverses a packet_header passed to it */
void RoutingProtocolImpl::reverse_header(void *packet_header){
  struct packet_header *h = (struct packet_header *) packet_header;
  
  h->size = ntohs(h->size);
  h->source_id = ntohs(h->source_id);
  h->dest_id = ntohs(h->dest_id);
}
