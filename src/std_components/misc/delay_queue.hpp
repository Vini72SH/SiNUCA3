#ifndef SINUCA3_DELAY_QUEUE_HPP
#define SINUCA3_DELAY_QUEUE_HPP

//
// Copyright (C) 2025  HiPES - Universidade Federal do Paraná
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/**
 */

#include <engine/component.hpp>
#include <sinuca3.hpp>
#include <utils/circular_buffer.hpp>

#include "config/config.hpp"
#include "utils/logging.hpp"

template <typename Type>
class DelayQueue : public Component<Type> {
  private:
    struct Input {
        Type elem;
        unsigned long removeAt;
        void SetRemoval(unsigned long l) { this->removeAt = l; }
    };

    Input queueFirst;
    Input elemToInsert;
    Input elemToRemove;
    CircularBuffer delayBuffer;
    Component<Type>* sendTo;
    unsigned long cyclesClock;
    long occupation;
    long throughput;
    long delay;
    int sendToId;

    int CalculateDelayBufferSize();
    int Enqueue();
    int Dequeue();

  public:
    DelayQueue();
    virtual int SetConfigParameter(const char* parameter, ConfigValue value);
    virtual void PrintStatistics() {}
    virtual int FinishSetup();
    virtual void Clock();
    virtual ~DelayQueue();
};

template <typename Type>
DelayQueue<Type>::DelayQueue()
    : sendTo(NULL), cyclesClock(0), occupation(0), throughput(0), delay(0) {}

template <typename Type>
DelayQueue<Type>::~DelayQueue() {
    this->delayBuffer.Deallocate();
}

template <typename Type>
int DelayQueue<Type>::CalculateDelayBufferSize() {
    return this->delay * this->throughput;
}

template <typename Type>
int DelayQueue<Type>::Enqueue() {
    if (this->delayBuffer.IsFull()) {
        return 1;
    }
    if (this->occupation == 0) {
        this->queueFirst = this->elemToInsert;
    } else {
        this->delayBuffer.Enqueue(&this->elemToInsert);
    }
    this->occupation++;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::Dequeue() {
    if (this->occupation < 1) {
        return 1;
    }
    if (this->queueFirst.removeAt > this->cyclesClock) {
        return 1;  // Not yet time to remove
    }
    this->elemToRemove = this->queueFirst;
    if (this->occupation > 1) {
        this->delayBuffer.Dequeue(&this->queueFirst);
    }
    this->occupation--;
    return 0;
}

template <typename Type>
int DelayQueue<Type>::SetConfigParameter(const char* param, ConfigValue val) {
    if (!strcmp(param, "delay")) {
        if (val.type != ConfigValueTypeInteger) {
            return 1;
        }
        long delay = val.value.integer;
        if (delay < 0) {
            return 1;
        }
        this->delay = delay;
        return 0;
    }
    if (!strcmp(param, "throughput")) {
        if (val.type != ConfigValueTypeInteger) {
            return 1;
        }
        long throughput = val.value.integer;
        if (throughput <= 0) {
            return 1;
        }
        this->throughput = throughput;
        return 0;
    }
    if (!strcmp(param, "sendTo")) {
        if (val.type != ConfigValueTypeComponentReference) {
            return 1;
        }
        Component<Type>* comp =
            dynamic_cast<Component<Type>*>(val.value.componentReference);
        if (comp == NULL) {
            return 1;
        }
        this->sendTo = comp;
        return 0;
    }
    return 1;
}

template <typename Type>
int DelayQueue<Type>::FinishSetup() {
    if (this->delay == 0 || this->throughput == 0 || this->sendTo == NULL) {
        return 1;
    }
    this->sendToId = this->sendTo->Connect(this->throughput);
    this->delayBuffer.Allocate(this->CalculateDelayBufferSize(), sizeof(Input));
    return 0;
}

template <typename Type>
void DelayQueue<Type>::Clock() {
    long totalConnections = this->GetNumberOfConnections();

    this->cyclesClock++;
    if (this->cyclesClock + (unsigned long)this->delay == (unsigned long)~0) {
        SINUCA3_ERROR_PRINTF(
            "Congratulations! You've achieved something deemed impossible "
            "[%lu] cycles\n",
            this->cyclesClock);
    }

    if (this->delay == 0) {
        Type* elem = &this->elemToInsert.elem;
        for (long i = 0; i < totalConnections; i++) {
            while (!this->ReceiveRequestFromConnection(i, elem)) {
                if (this->sendTo->SendRequest(sendToId, elem)) return;
            }
        }
        return;
    }

    while (!this->Dequeue()) {
        this->sendTo->SendRequest(sendToId, &this->elemToRemove.elem);
    }
    this->elemToInsert.SetRemoval(this->cyclesClock + this->delay);
    for (long i = 0; i < totalConnections; i++) {
        Type* elem = &this->elemToInsert.elem;
        while (!this->ReceiveRequestFromConnection(i, elem)) {
            if (this->Enqueue()) return;
        }
    }
}

#ifndef NDEBUG
int TestDelayQueue();
#endif

#endif  // SINUCA3_DELAY_QUEUE