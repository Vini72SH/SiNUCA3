//
// Copyright (C) 2024  HiPES - Universidade Federal do Paraná
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
 * @file linkable.cpp
 * @brief Implementation of the Linkable class.
 */

#include "linkable.hpp"

void sinuca::engine::Connection::CreateBuffers(int bufferSize,
                                               int messageSize) {
    this->bufferSize = bufferSize;
    this->messageSize = messageSize;

    this->requestBuffers[0].Allocate(bufferSize, messageSize);
    this->requestBuffers[1].Allocate(bufferSize, messageSize);

    this->responseBuffers[0].Allocate(bufferSize, messageSize);
    this->responseBuffers[1].Allocate(bufferSize, messageSize);
};

inline int sinuca::engine::Connection::GetBufferSize() const {
    return this->bufferSize;
};

inline int sinuca::engine::Connection::GetMessageSize() const {
    return this->messageSize;
};

void sinuca::engine::Connection::SendRequest(char id, void* message) {
    this->requestBuffers[id].Enqueue(message);
};

void sinuca::engine::Connection::SendResponse(char id, void* message) {
    this->responseBuffers[id].Enqueue(message);
};

void* sinuca::engine::Connection::ReceiveRequest(char id) {
    return this->requestBuffers[id].Dequeue();
};

void* sinuca::engine::Connection::ReceiveResponse(char id) {
    return this->responseBuffers[id].Dequeue();
};

sinuca::engine::Linkable::Linkable(int messageSize)
    : messageSize(messageSize), numberOfConnections(0){};

void sinuca::engine::Linkable::AllocateConnectionsBuffer(
    long numberOfConnections) {
    this->numberOfConnections = numberOfConnections;
    this->connections.reserve(numberOfConnections);
};

void sinuca::engine::Linkable::DeallocateConnectionsBuffer() {
    for (int i = 0; i < numberOfConnections; ++i) {
        delete this->connections[i];
    }
};

void sinuca::engine::Linkable::AddConnection(Connection* newConnection) {
    int connectionsSize = this->connections.size();
    this->connections.push_back(newConnection);

    if (connectionsSize > this->numberOfConnections)
        this->numberOfConnections = connectionsSize;
};

int sinuca::engine::Linkable::Connect(int bufferSize) {
    int index = this->connections.size();

    Connection* newConnection = new Connection();
    newConnection->CreateBuffers(bufferSize, this->messageSize);
    this->AddConnection(newConnection);

    return index;
};

void sinuca::engine::Linkable::SendRequestToLinkable(Linkable* dest,
                                                     int connectionID,
                                                     void* message) {
    dest->connections[connectionID]->SendRequest(DEST_ID, message);
};

void sinuca::engine::Linkable::SendResponseToLinkable(Linkable* dest,
                                                      int connectionID,
                                                      void* message) {
    dest->connections[connectionID]->SendResponse(DEST_ID, message);
};

void* sinuca::engine::Linkable::ReceiveRequestFromLinkable(Linkable* dest,
                                                           int connectionID) {
    return dest->connections[connectionID]->ReceiveRequest(SOURCE_ID);
};

void* sinuca::engine::Linkable::ReceiveResponseFromLinkable(Linkable* dest,
                                                            int connectionID) {
    return dest->connections[connectionID]->ReceiveResponse(SOURCE_ID);
};

void sinuca::engine::Linkable::SendRequestToConnection(int connectionID,
                                                       void* message) {
    this->connections[connectionID]->SendRequest(SOURCE_ID, message);
};

void sinuca::engine::Linkable::SendResponseToConnection(int connectionID,
                                                        void* message) {
    this->connections[connectionID]->SendResponse(SOURCE_ID, message);
};

void* sinuca::engine::Linkable::ReceiveRequestFromConnection(int connectionID) {
    return this->connections[connectionID]->ReceiveRequest(DEST_ID);
};

void* sinuca::engine::Linkable::ReceiveResponseFromConnection(
    int connectionID) {
    return this->connections[connectionID]->ReceiveResponse(DEST_ID);
};

void sinuca::engine::Linkable::PreClock() {}
void sinuca::engine::Linkable::PosClock() {}

sinuca::engine::Linkable::~Linkable(){};
