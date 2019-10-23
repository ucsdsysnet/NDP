
#include "config.h"
#include <sstream>
#include <strstream>
#include <iostream>
#include <string.h>
#include <math.h>
#include "network.h"
#include "randomqueue.h"
//#include "subflow_control.h"
#include "shortflows.h"
#include "pipe.h"
#include "eventlist.h"
#include "logfile.h"
#include "loggers.h"
#include "clock.h"
#include "tcp.h"
#include "compositequeue.h"
#include "firstfit.h"
#include "topology.h"
#include "connection_matrix.h"
#include "dctcp.h"
#include "dctcp_transfer.h"
//#include "vl2_topology.h"

#include "fat_tree_topology.h"
//#include "oversubscribed_fat_tree_topology.h"
//#include "multihomed_fat_tree_topology.h"
//#include "star_topology.h"
//#include "bcube_topology.h"
#include <list>

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

#define MAX_START_DELAY_US 0
#define DEFAULT_PACKET_SIZE 9000
#define DEFAULT_NODES 432
#define DEFAULT_QUEUE_SIZE 100
uint32_t RTT = 1; // this is per link delay in us; identical RTT microseconds = 0.001 ms
FirstFit* ff = NULL;

unsigned int subflow_count = 1;

string ntoa(double n);
string itoa(uint64_t n);

EventList eventlist;

int no_of_conns = 0, no_of_nodes = DEFAULT_NODES, ssthresh = 15, failed_links = 0,
    flowsize=0, seed_val=0, starting_wnd = 10, rto = 10;
uint32_t ack_delay=0, host_delay=0;
mem_b queuesize;
stringstream filename(ios_base::out);

void exit_error(char* progr, char *param) {
    cerr << "Bad parameter: " << param << endl;
    cerr << "Usage " << progr << " (see src code for parameters)" << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route* rt){
    for (unsigned int i=1;i<rt->size()-1;i+=2){
        RandomQueue* q = (RandomQueue*)rt->at(i);
        if (q!=NULL)
            paths << q->str() << " ";
        else
            paths << "NULL ";
    }

    paths<<endl;
}

class StopLogger : public EventSource {
public:
    StopLogger(EventList& eventlist, const string& name) : EventSource(eventlist, name) {};
    void doNextEvent() {
        //nothing to do, just prevent flow restarting
    }
private:
};

int parse_args(int argc, char** argv) {
    int i = 1;
    while (i<argc) {
        if (i == argc-1) exit_error(argv[0], argv[i]);
        if (!strcmp(argv[i],"-o")){
            filename.str(std::string());
            filename << argv[i+1];
            i++;
        } else if (!strcmp(argv[i],"-sub")){
            subflow_count = atoi(argv[i+1]);
            i++;
        } else if (!strcmp(argv[i],"-conns")){
            no_of_conns = atoi(argv[i+1]);
            cout << "no_of_conns "<<no_of_conns << endl;
            i++;
        } else if (!strcmp(argv[i],"-nodes")){
            no_of_nodes = atoi(argv[i+1]);
            cout << "no_of_nodes "<<no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i],"-ssthresh")){
            ssthresh = atoi(argv[i+1]);
            cout << "ssthresh "<< ssthresh << endl;
            i++;
        } else if (!strcmp(argv[i],"-queuesize")){
            queuesize = memFromPkt(atoi(argv[i+1]));
            cout << "queuesize "<< queuesize << endl;
            i++;
        } else if (!strcmp(argv[i],"-flowsize")){
            flowsize = atoi(argv[i+1]) * Packet::data_packet_size();
            cout << "flowsize "<< flowsize << endl;
            i++;
        } else if (!strcmp(argv[i],"-fail")){
            failed_links = atoi(argv[i+1]);
            cout << "failed_links "<<failed_links << endl;
            i++;
        } else if (!strcmp(argv[i],"-seed")){
            seed_val = atoi(argv[i+1]);
            cout << "seed " << seed_val << endl;
            i++;
        } else if (!strcmp(argv[i],"-ackdelay")) {
            ack_delay = atoi(argv[i+1]);
            cout << "ack_delay " << ack_delay << endl;
            i++;
        } else if (!strcmp(argv[i],"-hostdelay")) {
            host_delay = atoi(argv[i+1]);
            cout << "host_delay " << host_delay << endl;
            i++;
        } else if (!strcmp(argv[i],"-startingwnd")) {
            starting_wnd = atoi(argv[i+1]);
            cout << "starting_wnd " << starting_wnd << endl;
            i++;
        } else if (!strcmp(argv[i],"-rto")) {
            rto = atoi(argv[i+1]);
            cout << "rto " << rto << endl;
            i++;
        } else {
            exit_error(argv[0], argv[i]);
            return -1;
        }
        i++;
    }
    return 0;
}

int main(int argc, char **argv) {
    Packet::set_packet_size(DEFAULT_PACKET_SIZE);
    queuesize = memFromPkt(DEFAULT_QUEUE_SIZE);
    eventlist.setEndtime(timeFromSec(2));
    flowsize = Packet::data_packet_size()*50;
    Clock c(timeFromSec(5 / 100.), eventlist);

    if(parse_args(argc, argv))
        exit(1);

    srand(seed_val ? seed_val : time(NULL));

    cout << "Using subflow count " << subflow_count <<endl;
    cout << "Logging to " << filename.str() << endl;

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths){
        cout << "Can't open for writing paths file!"<<endl;
        exit(1);
    }
#endif

    Logfile logfile(filename.str(), eventlist);
    logfile.setStartTime(timeFromSec(0));

    TcpSinkLoggerSampling sinkLogger = TcpSinkLoggerSampling(timeFromMs(10), eventlist);
    logfile.addLogger(sinkLogger);
    TcpTrafficLogger traffic_logger = TcpTrafficLogger();
    logfile.addLogger(traffic_logger);
    TcpRtxTimerScanner tcpRtxScanner(timeFromMs(10), eventlist);
    StopLogger stop_logger = StopLogger(eventlist, "stop_logger");

    Route* routeout, *routein;
    double extrastarttime;
    int tot_subs = 0;
    int cnt_con = 0;
    int dest;

#if USE_FIRST_FIT
    if (subflow_count==1){
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL),eventlist);
    }
#endif

#ifdef FAT_TREE
    FatTreeTopology* top = new FatTreeTopology(no_of_nodes, queuesize, &logfile,
                                               &eventlist,ff,ECN,failed_links);
#endif

    vector<const Route*>*** net_paths;
    net_paths = new vector<const Route*>**[no_of_nodes];
    vector<int>* destinations;

    int* is_dest = new int[no_of_nodes];

    for (int i=0;i<no_of_nodes;i++){
        is_dest[i] = 0;
        net_paths[i] = new vector<const Route*>*[no_of_nodes];
        for (int j = 0;j<no_of_nodes;j++)
            net_paths[i][j] = NULL;
    }

#if USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
#endif

    // used just to print out stats data at the end
    list <const Route*> routes;

    ConnectionMatrix* conns = new ConnectionMatrix(no_of_nodes);
    cout << "Running perm with " << no_of_conns << " connections" << endl;
    // conns->setRandomGuaranteed(no_of_conns, seed_val);
    conns->setIncast(no_of_conns, 0);

    TcpSrc* tcpSrc;
    TcpSink* tcpSnk;
    map<int,vector<int>*>::iterator it;
    int connID = 0;
    for (it = conns->connections.begin(); it!=conns->connections.end();it++){
        int src = (*it).first;
        destinations = (vector<int>*)(*it).second;
        vector<int> subflows_chosen;

        for (unsigned int dst_id = 0;dst_id<destinations->size();dst_id++){
            connID++;
            dest = destinations->at(dst_id);

            if (!net_paths[src][dest]) {
                vector<const Route*>* paths = top->get_paths(src,dest);
                net_paths[src][dest] = paths;
                for (unsigned int i = 0; i < paths->size(); i++) {
                    routes.push_back((*paths)[i]);
                }
            }
            if (!net_paths[dest][src]) {
                vector<const Route*>* paths = top->get_paths(dest,src);
                net_paths[dest][src] = paths;
            }

            for (int connection=0;connection<1;connection++){
                cnt_con ++;

                tcpSrc = new DCTCPSrcTransfer(NULL, NULL, eventlist, flowsize, NULL, &stop_logger, timeFromUs(host_delay));
                tcpSnk = new DCTCPSinkTransfer(eventlist, timeFromUs(ack_delay));

                tcpSrc->set_starting_wnd(starting_wnd);
                tcpSrc->set_ssthresh(ssthresh*Packet::data_packet_size());
                // tcpSrc->set_flowsize(flowsize*Packet::data_packet_size());
                tcpSrc->_rto = timeFromMs(rto);

                tcpSrc->setName("dctcp_" + ntoa(src) + "_" + ntoa(dest) + "_" + std::to_string(connID));
                logfile.writeName(*tcpSrc);

                tcpRtxScanner.registerTcp(*tcpSrc);

                int choice = 0;
#ifdef FAT_TREE
                choice = rand()%net_paths[src][dest]->size();
#endif

                subflows_chosen.push_back(choice);
                if (choice>=net_paths[src][dest]->size()){
                    printf("Weird path choice %d out of %lu\n",choice,net_paths[src][dest]->size());
                    exit(1);
                }

#if PRINT_PATHS
                for (int ll=0;ll<net_paths[src][dest]->size();ll++){
                    paths << "Route from "<< ntoa(src) << " to " << ntoa(dest) << "  (" << ll << ") -> " ;
                    print_path(paths,net_paths[src][dest]->at(ll));
                }
#endif

                routeout = new Route(*(net_paths[src][dest]->at(choice)));
                routeout->push_back(tcpSnk);

                routein = new Route();

                routein = new Route(*top->get_paths(dest,src)->at(choice));
                routein->push_back(tcpSrc);

                // TODO swap this with another random start method.
                extrastarttime = MAX_START_DELAY_US * drand();

                tcpSrc->connect(*routeout, *routein, *tcpSnk, timeFromUs(extrastarttime));
                sinkLogger.monitorSink(tcpSnk);
            }
        }
    }


    cout << "Mean number of subflows " << ntoa((double)tot_subs/cnt_con)<<endl;

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC*CORE_TO_HOST) + " pkt/sec");
    //logfile.write("# buffer = " + ntoa((double) (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    // GO!
    while (eventlist.doNextEvent()) { }

    cout << "Done" << endl;
}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
