#include <queue>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <iostream>

#define M_PI 3.1415926

#define numOfRelayNodes       50      // ���ե� �ݧ�
#define numOfSinkNodes        1
#define numOfSourceNodes      1
#define numOfNodes            (numOfRelayNodes+numOfSinkNodes+numOfSourceNodes)
#define numOfPackets          150      // ���ե� �ݧ�
#define numOfReferredPackets  100      // ���ե� �ݧ�

#define packetSize            3000     // unit in bytes
#define packetDataLength      2988 

#define dimensionX            1000     // unit in meters
#define dimensionY            1000     // ���ե� �ݧ�
#define dimensionZ            1000     // ��ڦҼ{ 1000-3000m

#define nodeSpeedMin          0.0001   // unit in meters per ms
#define nodeSpeedMax          0.0005        

#define powerIdling           0.000008 // unit in watts (J/ms) 8mW
#define powerReceiving        0.00075  // 0.75W
#define powerSending          0.002    // 2W

#define energy_MAX            100      // unit in Joule (J) (�C�Ӹ`�I��l���q�q?)

#define deltaValue            100      // unit in meters --> �i�ۥѽվ�ѼơA�Ѧ�DBR (delta�V�pholding�V��)
#define transmissionRangeMax  350      // unit in meters (���n�Ѽ�)
#define propogationSpeed      1.5      // unit in meters per ms
#define bitRate               10       // unit in bits per ms

#define simulationTime        6000000  // unit in ms (10 min)
#define extendedTime          600000   // �̫�@�ӫʥ]�e�X��A�����@�p�q�ɶ�
#define timeslotLength        100      // unit in ms
#define positionUpdateSlot    1000     // unit in ms

#define sourcePacketRate      1000     // unit in ms per packet (constant bit rate, CBR)

#define event_ARRIVAL         0
#define event_DEPARTURE       1

#define node_type_SOURCE      0
#define node_type_SINK        1
#define node_type_RELAY       2

#define collisionEnabled      1        // ����collision�ɱN���]��"0"

//-----------------------------------------------------------

#define traceFileName_0         "DBR_0_channel_model.log" // channel model (transmitter power, node distance, bit error rate)
#define traceFileName_1         "DBR_1_neighborhood.log"  // neighbors of each node
#define traceFileName_2         "DBR_2_events_and_collisions.log" // number of events and number of collisions
#define traceFileName_3         "DBR_3.log"                       // �O�d
#define traceFileName_4         "DBR_4_max_number_collisions.log" // max number of collisions of one slot
#define traceFileName_5         "DBR_5_info_departure_event.log"  // information of departure events
#define traceFileName_6         "DBR_6_info_arrival_event.log"    // information of arrival events
#define traceFileName_7         "DBR_7.log"                       // �O�d
#define traceFileName_8         "DBR_8_hop_count.log"             // hop count of each packet
#define traceFileName_9         "DBR_9_energy_consumption.log"    // energy consumption of each node
#define traceFileName_10        "DBR_10_node_position.log"        // position of source, sink, relay nodes
#define traceFileName_11        "DBR_11_node_position_mobile.log" // position of nodes (different time slots)

FILE *tFile0 = fopen(traceFileName_0, "w");  
FILE *tFile1 = fopen(traceFileName_1, "w");
FILE *tFile2 = fopen(traceFileName_2, "w");
FILE *tFile3 = fopen(traceFileName_3, "w");
FILE *tFile4 = fopen(traceFileName_4, "w");
FILE *tFile5 = fopen(traceFileName_5, "w");
FILE *tFile6 = fopen(traceFileName_6, "w");
FILE *tFile7 = fopen(traceFileName_7, "w");
FILE *tFile8 = fopen(traceFileName_8, "w");  
FILE *tFile9 = fopen(traceFileName_9, "w");  
FILE *tFile10 = fopen(traceFileName_10, "w"); 
FILE *tFile11 = fopen(traceFileName_11, "w"); 

using namespace std;

typedef struct DBR_Packet  // Packet format
{
    int senderNodeID, packetSN, nodeDepth;
    int hop_count;                          // number of hops (from source to sink)
    //char packetData[packetDataLength];    // ���O����Ŷ��G������

    DBR_Packet() {}
    DBR_Packet(int s, int n, int d, int hc) {
        senderNodeID = s;
        packetSN = n;
        nodeDepth = d;
        hop_count = hc;
        //strcpy(packetData, pd);
    }
} DBRpacket;

/* �D�֤ߤ�k(�B����bug), �G������
typedef struct PQ_Item    // Priority queue (Q1) item
{
    int scheduledSendingTime;
    bool bScheduled;
    PQ_Item() {
        scheduledSendingTime = INT_MAX;
        bScheduled = false;
    }
} Q1_Item;
*/

typedef struct PHB_Item   // Packet history buffer (Q2) item
{
    //int senderNodeID, packetSN;
    bool bBuffered;

    PHB_Item() {
        bBuffered = false;
    }

} Q2_Item;

typedef struct DBR_Node  // Data structure for node
{
    int nX, nY, nZ;
    int nodeType;
    float currentEnergy, consumedEnergy;   // energy of node

    DBR_Node() {}
    DBR_Node(int x, int y, int z, int t) {
        nX = x;
        nY = y;
        nZ = z;
        nodeType = t;  
        currentEnergy = energy_MAX;    // ��l�q�q
        consumedEnergy = 0;            // ��l�ӹq
    }
} DBRnode;

typedef struct Neighbor_Node  // Data structure for recording neighbor node
{
    int nodeID;
    int nodeDepth;
    float nodeDistance;
    float propogationDelay;
    float transmissionDelay;
    float totalDelay;

    Neighbor_Node() {}
    Neighbor_Node(int i, int dep, float dis) {
        nodeID = i;
        nodeDepth = dep;
        nodeDistance = dis;
        propogationDelay = nodeDistance / propogationSpeed;
        transmissionDelay = (float)(packetSize*8) / bitRate;
        totalDelay = propogationDelay + transmissionDelay;
    }
} NeighborNode;

struct Neighbor_Sort_Distance
{
    // sort by distance
    inline bool operator() (const Neighbor_Node& struct1, const Neighbor_Node& struct2)
    {
        return (struct1.nodeDistance < struct2.nodeDistance);
    }
} NeighborSortDistance;

struct Neighbor_Sort_Depth
{
    // sort by Depth
    inline bool operator() (const Neighbor_Node& struct1, const Neighbor_Node& struct2)
    {
        return (struct1.nodeDepth < struct2.nodeDepth);
    }
} NeighborSortDepth;

typedef struct Event_Item  // Data structure for event item
{
    int eventType;
    int occurringTime;

    /*
	   Arrival Event: nodeID����destination 
       Departure Event: nodeID����source
    */
    int nodeID; 

    DBRpacket packet;

    Event_Item() {}
    Event_Item(int t, int o, int i, DBRpacket p) {
        eventType = t;
        occurringTime = o;
        nodeID = i;
        packet = p;
    }
} EventItem;

struct Event_Sort_OccurringTime
{
    // sort by occurring time
    inline bool operator() (const Event_Item& struct1, const Event_Item& struct2)
    {
        return (struct1.occurringTime < struct2.occurringTime);
    }
} EventSortOccurringTime;

struct Event_Sort_PacketSN
{
    // sort by PacketSN
    inline bool operator() (const Event_Item& struct1, const Event_Item& struct2)
    {
        return (struct1.packet.packetSN < struct2.packet.packetSN);
    }
} EventSortPacketSN;

typedef struct Packet_Record_Item  // Packet format for packet record table (�̫�έp�ɨϥ�)
{
    int generatedTime, arrivedTime, delay, hopCount;
    bool isArrived;                // �O�_��Fsink
        
    Packet_Record_Item() {
        generatedTime = INT_MAX;
        arrivedTime = INT_MAX;
        delay = INT_MAX;
        hopCount = 0;             // �O�� hop count of each packet
        isArrived = false;
    }
} PacketRecordItem;

typedef struct Node_Record_Item  // node record table (�̫�έp�ɨϥ�)
{
    int position_X, position_Y, position_Z;

    Node_Record_Item() {
        position_X = INT_MIN;
        position_Y = INT_MIN;
        position_Z = INT_MIN;           
    }
} NodeRecordItem;

//-----------------------------------------------------------
// �ŧi 1) sensor nodes 2) event queue 3) packet record table
DBRnode sensorNode[numOfNodes];
vector<EventItem> eventQueue;                                
PacketRecordItem packetRecordTable[numOfPackets];       // packet info table
NodeRecordItem nodeRecordTable[numOfNodes];             // node info table
//-----------------------------------------------------------

//-----------------------------------------------------------
// �ŧi 4) priority queue 5) history buffer (for each node)
//Q1_Item PriorityQueue[numOfNodes][numOfPackets];     // Priority queue (Q1)
Q2_Item HistoryBuffer[numOfNodes][numOfPackets];       // history buffer (Q2)
// �O��node i�O�_������(Q1)�ιL�h(Q2)��packet Sequence Number
//-----------------------------------------------------------

//-----------------------------------------------------------                                           
int packetSN; 
int depthThreshold;
int numOfArrivals;
int numOfDepartures;
int nPositionUpdated;
int totalCollisions;                   
int sourceSendingTime[numOfSourceNodes];   // unit in ms
int currentTime;                           // unit in ms
bool bEndWithTime;                         // �O�_�]�����ɶ���ӵ���
int SinkPositionOption;                    // sink positions�O�w�]/���/�۰�
int SourcePositionOption;                  // source positions�O�w�]/���/�۰�
int executionTime;                         // �����{���w�p���浲�����ɶ�
int max_num_of_collision_in_one_slot;      // just for debug
int collision_count;                       // just for debug
//-----------------------------------------------------------

void printToCheckNeighborNodes();                 
void printToCheckChannelModelValue();             
void collisionPrinting(int nCollisions);   
void endingMessage();                              
void centralizedPreprocessing();
void establishSensorNetworkGraph();
void generateInitialDepartureEventsForSource();
int getNumberOfEventsInTimeslot(int currentTime);

void dataPacketProcessing(int iEvent);     

bool checkPacketArriveSink(int eIndex);
void DBR_PacketForwardingAlgorithm(int eIndex);
void departurePacketProcessing(int iEvent);
void generateArrivalEventsForNeighbors(int eIndex);
vector<NeighborNode> scanNeighbors(int nodeID, int txRange);
bool determineSendingSuccessByModels(float nodeDistance, bool printValue);
bool checkReceivedPacketHeader(EventItem currentEvent);
float computeHoldingTime(int transRangeMax, float propSpeed, float delta, int delta_d);
void removeEvents(int numberOfProcessedEvents);
int removeCollisionEvents(int nEvents);
bool generateNewDepartureEventsForSource(int iNode);
void updateRelayNodePosition();
void calculateStatistics();
float calculateDeliveryRatio();
float calculateEnergyConsumption();
float calculateTotalDelay();

//-----------------------------------------------------------
void printPacketHopCount();                              
void printNodeEnergyCurrentTimeslot(int timeslot);       
void printCenterTableCurrentTimeslot(int timeslot);   // record node information (position)   
float get_rand(float lower, float upper);
//-----------------------------------------------------------
void idleEnergyConsumption();      
void checkAndGenerateNewPacket();        
void checkNodePositionUpdate();    
//-----------------------------------------------------------
float calculateNumOfArrived();
float calculateDelayOfNonArrived();
void updatePriorityQueue(int nodeID, int packetSN, int occuringTime);
void updateHistoryBuffer(int nodeID, int packetSN);
//-----------------------------------------------------------
void trace(bool bIsFile);
void closeLogFiles();      
//void priorityQueueOperation(int currentTime, int &ST, DBRpacket newPacket);  // �Ȥ�����
//-----------------------------------------------------------
void initialMessageAndInsert();            
void initializeParameters();               
void initializeNodePosition();          
void setSinkDefaultPositions(); 