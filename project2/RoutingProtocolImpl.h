#ifndef ROUTINGPROTOCOLIMPL_H
#define ROUTINGPROTOCOLIMPL_H

#include "RoutingProtocol.h"
#include "Node.h"
#include <unordered_map>

class RoutingProtocolImpl : public RoutingProtocol {
  public:
    RoutingProtocolImpl(Node *n);
    ~RoutingProtocolImpl();

    void init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type);
    // As discussed in the assignment document, your RoutingProtocolImpl is
    // first initialized with the total number of ports on the router,
    // the router's ID, and the protocol type (P_DV or P_LS) that
    // should be used. See global.h for definitions of constants P_DV
    // and P_LS.

    void handle_alarm(void *data);
    // As discussed in the assignment document, when an alarm scheduled by your
    // RoutingProtoclImpl fires, your RoutingProtocolImpl's
    // handle_alarm() function will be called, with the original piece
    // of "data" memory supplied to set_alarm() provided. After you
    // handle an alarm, the memory pointed to by "data" is under your
    // ownership and you should free it if appropriate.

    void recv(unsigned short port, void *packet, unsigned short size);
    // When a packet is received, your recv() function will be called
    // with the port number on which the packet arrives from, the
    // pointer to the packet memory, and the size of the packet in
    // bytes. When you receive a packet, the packet memory is under
    // your ownership and you should free it if appropriate. When a
    // DATA packet is created at a router by the simulator, your
    // recv() function will be called for such DATA packet, but with a
    // special port number of SPECIAL_PORT (see global.h) to indicate
    // that the packet is generated locally and not received from 
    // a neighbor router.

    void send_dv_update(unsigned short from, unsigned short to, unsigned short on_port);
    // Sends a dv_update message from router "from" to router "to" on port "on_port"

    void reverse_header(void *packet_header);
    // Reverses the byte order of the components in the packet_header passed to it

    void update_all_through(unsigned short port, int changed, unsigned short except);
    // Changes the length of all paths through port by change, does not
    // change the distance for paths to except_for (useful if, for example, 
    // except_for the caller's neighbor and it's distance could have already been updated)
 

 private:
    Node *sys; // To store Node object; used to access GSR9999 interfaces 
    /**  
     * The keys are discovered nodes, the values are the ports that the current shortest
     * path to those nodes goes through.
     */
    unordered_map<unsigned short, unsigned short> id_port_map;
    /**
     * The keys are the ids of discovered nodes, the values are the lengths of the 
     * shortest path to those nodes, measured in ms/roundtrip
     */
    unordered_map<unsigned short, unsigned int> id_dist_map;
    /**
     * The keys are the ids of neighbors, the values are the port that router
     * lies across
     */
    unordered_map<unsigned short, unsigned short> neighbors_port_map;
    /**
     * The keys are the ids of discovered nodes, the values are the time
     * in seconds since the current router got information about that node
     */
    unordered_map<unsigned short, unsigned short> id_updated_map;
    unsigned short num_dif_ports;
    unsigned short my_id;
    eProtocolType my_protocol_type;
};

#endif

