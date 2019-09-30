#ifndef TCP_PERIODIC_H
#define TCP_PERIODIC_H

/*
 * A recurrent TCP flow, source and sink
 */

#include <list>
#include "config.h"
#include "network.h"
#include "eventlist.h"
#include "tcp.h"

class TcpSinkPeriodic;

class TcpSrcPeriodic: public TcpSrc {
public:
  TcpSrcPeriodic(TcpLogger* logger, TrafficLogger* pktLogger, EventList &eventlist,simtime_picosec active=0,simtime_picosec idle=0);

  void connect(route_t& routeout, route_t& routeback, TcpSink& sink, simtime_picosec starttime);

  void receivePacket(Packet& pkt);
  void reset();
  void doNextEvent();

// should really be private, but loggers want to see:
  simtime_picosec _period,_active_time,_idle_time,_start_active,_end_active;
  bool _is_active;
};

class TcpSinkPeriodic : public TcpSink {
friend class TcpSrcPeriodic;
public:
        TcpSinkPeriodic(EventList &eventlist);
        TcpSinkPeriodic(EventList &eventlist, simtime_picosec ack_delay);

        void reset();
};

#endif
