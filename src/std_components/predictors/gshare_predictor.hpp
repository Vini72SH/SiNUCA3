#ifndef SINUCA3_GSHARE_PREDICTOR_HPP_
#define SINUCA3_GSHARE_PREDICTOR_HPP_

#include <sinuca3.hpp>
#include "engine/component.hpp"
#include "engine/default_packets.hpp"
#include <utils/circular_buffer.hpp>
#include <utils/bimodal_counter.hpp>

class GsharePredictor : public Component<PredictorPacket> {
  private:
    struct Request {
        unsigned long index;
    };

    BimodalCounter* entries;
    CircularBuffer requestsQueue;
    unsigned long globalBranchHistReg;
    unsigned long numberOfEntries;
    unsigned long numberOfPredictions;
    unsigned long numberOfWrongpredictions;
    unsigned int requestsQueueSize;
    unsigned int indexBitsSize;

    Component<PredictorPacket>* sendTo;
    int sendToId;

    int Allocate();
    void Deallocate();
    int RoundNumberOfEntries(unsigned long requestedSize);
    void UpdateEntry(unsigned long idx, bool direction);
    void UpdateGlobBranchHistReg(bool direction);
    bool QueryEntry(unsigned long idx);
    unsigned long CalculateIndex(unsigned long addr);

    inline void EnqueueReq(Request req) { this->requestsQueue.Enqueue(&req); }
    inline void DequeueReq(Request* req) { this->requestsQueue.Dequeue(req); }

  public:
    GsharePredictor();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics();
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~GsharePredictor(); 
};

#endif  // SINUCA3_GSHARE_PREDICTOR