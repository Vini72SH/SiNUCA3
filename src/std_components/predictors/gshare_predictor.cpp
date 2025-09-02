

#include "gshare_predictor.hpp"
#include "config/config.hpp"
#include "engine/default_packets.hpp"
#include "utils/logging.hpp"
#include <cmath>

const bool NTAKEN = 0;
const bool TAKEN = 1;

GsharePredictor::GsharePredictor() : entries(NULL), globalBranchHistReg(0), numberOfEntries(0), numberOfPredictions(0), numberOfWrongpredictions(0), requestsQueueSize(0), indexBitsSize(0) {}

GsharePredictor::~GsharePredictor() {
    this->Deallocate();
}

int GsharePredictor::Allocate() {
    this->entries = new BimodalCounter[this->numberOfEntries];
    if (!this->entries) {
        SINUCA3_ERROR_PRINTF("Gshare failed to allocate Bim\n");
        return 1;
    }
    this->requestsQueue.Allocate(this->requestsQueueSize, sizeof(Request));
    return 0;
}

void GsharePredictor::Deallocate() {
    if (this->entries) {
        delete this->entries;
        this->entries = NULL;
    }
    this->requestsQueue.Deallocate();
}

int GsharePredictor::RoundNumberOfEntries(unsigned long requestedSize) {
    int bits = (unsigned int)floor(log2(requestedSize));
    if (bits == 0) {
        return 1;
    }
    this->numberOfEntries = 1 << bits;
    this->indexBitsSize = bits;
    return 0;
}

void GsharePredictor::UpdateEntry(unsigned long idx, bool direction) {
    bool pred = this->entries[idx].GetPrediction();
    if (pred != direction) {
        this->numberOfWrongpredictions++;
    }
    this->entries[idx].UpdatePrediction(direction);
}

void GsharePredictor::UpdateGlobBranchHistReg(bool direction) {
    this->globalBranchHistReg <<= 1;
    if (direction == TAKEN) {
        this->globalBranchHistReg |= 1;
    }
}

bool GsharePredictor::QueryEntry(unsigned long idx) {
    this->numberOfPredictions++;
    return (this->entries[idx].GetPrediction() == TAKEN);
}

unsigned long GsharePredictor::CalculateIndex(unsigned long addr) {
    return (this->globalBranchHistReg ^ addr) & this->indexBitsSize;
}

int GsharePredictor::SetConfigParameter(const char* parameter, ConfigValue value) {
    if (strcmp(parameter, "numberOfEntries") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            return 1;
        }
        unsigned long requestedNumberOfEntries = value.value.integer;
        if (requestedNumberOfEntries <= 1) {
            return 1;
        }
        return 0;
    }
    if (strcmp(parameter, "requestsQueueSize") == 0) {
        if (value.type != ConfigValueTypeInteger) {
            return 1;
        }
        this->requestsQueueSize = value.value.integer;
        return 0;
    }
    SINUCA3_ERROR_PRINTF("Gshare predictor got unkown parameter\n");
    return 1;
}

void GsharePredictor::PrintStatistics() {
    SINUCA3_DEBUG_PRINTF("Gshare table size [%lu]\n", this->numberOfEntries);
    SINUCA3_LOG_PRINTF("Gshare number of predictions [%lu]\n", this->numberOfPredictions);
    SINUCA3_LOG_PRINTF("Gshare percentage of wrong predictions [%lu]\n", (this->numberOfWrongpredictions / this->numberOfPredictions));
}

int GsharePredictor::FinishSetup() {
    if (this->Allocate()) {
        return 1;
    }
    return 0;
}

void GsharePredictor::Clock() {
    PredictorPacket packet;

    long totalConnections = this->GetNumberOfConnections();
    for (long i = 0; i < totalConnections; i++) {
        while (this->ReceiveRequestFromConnection(i, &packet) == 0) {
            switch (packet.type) {
                case PredictorPacketTypeRequestQuery:
                    unsigned long addr = packet.data.requestQuery.staticInfo->opcodeAddress;
                    unsigned long idx = this->CalculateIndex(addr);
                    Request req = {idx};
                    this->EnqueueReq(req);
                    break;
                case PredictorPacketTypeRequestUpdate:
                    break;
                default:
                    SINUCA3_ERROR_PRINTF("Gshare invalid packet type\n");
            }
        }
    }
}