#include "core.hpp"

int Core::CoreSetup() {
    if (this->wasCoreSet)
        return 1;

    if (this->hasRequestedContext == false) {
        if (this->RequestCoreContext()) {
            return 1;
        }
        this->hasRequestedContext = true;
        return 1;
    } else {
        if (this->ReceiveCoreContext()) {
            return 1;
        }
    }

    this->HandEngineConnOwnershipToFetcher();
    this->ForwardContextIdentifierToMmu();

    return 0;
}

int Core::RequestCoreContext() {
    FetchPacket req;
    req.type = FetchPacketTypeSetup;
    return this->engine->SendRequest(this->engineConnectionID, &req);
};

int Core::ReceiveCoreContext() {
    FetchPacket req;
    if (this->engine->ReceiveResponse(this->engineConnectionID, &req) == 0) {
        this->contextId = req.contextId;
        return 0;
    }
    return 1;
}

int Core::HandEngineConnOwnershipToFetcher() {
    FetchPacket req;
    req.type = FetchPacketTypeSetup;
    req.connectionId = this->engineConnectionID;
    this->engineConnectionID = -1;
    return this->fetcher->SendRequest(this->fetcherConnectionID, &req);
}

int Core::ForwardContextIdentifierToMmu() {
    // Todo: Create mmu packet and forward context to mmu
    return 0;
}

void Core::PrintStatistics() {
    SINUCA3_LOG_PRINTF("Core [%d] inside context [%d].\n", this->coreId, this->contextId);
}

int Core::Configure(Config config) {
    if (config.ComponentReference("engine", &this->engine, true))
        return config.Error("engine", "Core needs to be connected to the engine.");
    if (config.ComponentReference("fetcher", &this->fetcher, true))
        return config.Error("fetcher", "Core needs to be connected to a fetcher.");
    if (config.ComponentReference("mmu", &this->memoryManagementUnit, true))
        return config.Error("mmu", "Core needs to be connected to a memory management unit.");

    this->fetcherConnectionID = this->fetcher->Connect(1);
    this->mmuConnectionID = this->memoryManagementUnit->Connect(1);
    this->engineConnectionID = this->Connect(0);

    this->coreId = this->engineConnectionID;

    return 0;
}

void Core::Clock() {
    if (!this->wasCoreSet) {
        if (!this->CoreSetup()) {
            this->wasCoreSet = true;
        }
    }
}