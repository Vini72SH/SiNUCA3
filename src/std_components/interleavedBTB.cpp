#include "interleavedBTB.hpp"
#include <cstdint>

btb_entry::btb_entry() : validBit(false), simplePredictor(nullptr) {};

void btb_entry::allocate() {
    validBit = false;
    tag = 0;
    fetchTarget = 0;
    simplePredictor = new BimodalPredictor();
};

bool btb_entry::getValid() {
    return validBit;
};

uint32_t btb_entry::getTag() {
    return tag;
};

uint32_t btb_entry::getTarget() {
    return fetchTarget;
};

bool btb_entry::getPrediction() {
    return simplePredictor->GetPrediction();
};

void btb_entry::setEntry(uint32_t tag, uint32_t fetchTarget) {
    this->validBit = true;
    this->tag = tag;
    this->fetchTarget = fetchTarget;
};

void btb_entry::updatePrediction(bool branchTaken) {
    simplePredictor->UpdatePrediction(branchTaken);
};

btb_entry::~btb_entry() {
    delete simplePredictor;
};

BranchTargetBuffer::BranchTargetBuffer() : Component<BTBMessage>(), instructionValidBits(nullptr), banks(nullptr) {};

uint32_t BranchTargetBuffer::calculateTag(uint32_t fetchAddress) {
    uint32_t tag = fetchAddress;
    tag = tag >> numBanks;

    return tag;
};

uint32_t BranchTargetBuffer::calculateIndex(uint32_t fetchAddress) {
    uint32_t index = fetchAddress;
    index = index >> numBanks;
    index = index & ((1 << numEntries) - 1);

    return index;
};

void BranchTargetBuffer::allocate(uint32_t numBanks, uint32_t numEntries) {
    this->numBanks = numBanks;
    this->numEntries = numEntries;
    this->totalBranches = 0;
    this->totalHits = 0;
    this->nextFetchBlock = 0;

    int totalBanks = (1 << numBanks);
    int totalEntries = (1 << numEntries);
    this->instructionValidBits = new bool[totalBanks];
    this->banks = new btb_bank[totalBanks];
    for (int bank = 0; bank < totalBanks; ++bank) {
        this->instructionValidBits[bank] = false;
        this->banks[bank] = new btb_entry[totalEntries];

        for (int entries = 0; entries < totalEntries; ++entries) {
            this->banks[bank][entries].allocate();
        }
    }
};

uint32_t BranchTargetBuffer::getNextFetchBlock() {
    return nextFetchBlock;
};

bool* BranchTargetBuffer::getInstructionValidBits() {
    return instructionValidBits;
};

void BranchTargetBuffer::registerNewBlock(uint32_t fetchAddress, uint32_t* fetchTargets) {
    uint32_t currentTag = calculateTag(fetchAddress);
    uint32_t index = calculateIndex(fetchAddress);

    for (uint32_t bank = 0; bank < numBanks; ++bank) {
        banks[bank][index].setEntry(currentTag, fetchTargets[bank]);
    }
};

TypeBTBMessage BranchTargetBuffer::fetchBTBEntry(uint32_t fetchAddress) {
    bool alocated = true;
    uint32_t nextBlock = 0;
    uint32_t currentTag = calculateTag(fetchAddress);
    uint32_t index = calculateIndex(fetchAddress);

    for (uint32_t i = 0; i < numBanks; ++i) {
        if (banks[i][index].getValid()) {
            if (banks[i][index].getTag() == currentTag) {
                nextBlock = banks[i][index].getTarget();
                instructionValidBits[i] = banks[i][index].getPrediction();
            } else {
                alocated = false;
                instructionValidBits[i] = true;
            }
        } else {
            alocated = false;
            instructionValidBits[i] = true;
        }
    }

    if (nextBlock) {
        nextFetchBlock = nextBlock;
    } else {
        nextFetchBlock = fetchAddress + (1 << numBanks);
    }

    if (alocated) {
        return ALLOCATED_ENTRY;
    }

    return UNALLOCATED_ENTRY;
};

void BranchTargetBuffer::updateBlock(uint32_t fetchAddress, bool* executedInstructions) {
    uint32_t currentTag = calculateTag(fetchAddress);
    uint32_t index = calculateIndex(fetchAddress);

    for (uint32_t bank = 0; bank < numBanks; ++bank) {
        if (banks[bank][index].getValid()) {
            if (banks[bank][index].getTag() == currentTag) {
                banks[bank][index].updatePrediction(executedInstructions[bank]);
            }
        }
    }
};

BranchTargetBuffer::~BranchTargetBuffer() {
    if (instructionValidBits) {
        delete[] instructionValidBits;
        instructionValidBits = nullptr;
    }

    int totalBanks = (1 << numBanks);
    if (banks) {
        for (int i = 0; i < totalBanks; ++i) {
            delete[] banks[i];
        }
        delete[] banks;
        banks = nullptr;
    }
};