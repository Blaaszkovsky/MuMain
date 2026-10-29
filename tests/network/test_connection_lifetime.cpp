#include "stdafx.h"
#include <winsock2.h>
#include <doctest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>

#include "Dotnet/Connection.h"

namespace
{
using namespace std::chrono_literals;
constexpr auto CompletionDeadline = 5s;
constexpr auto HeldCallbackObservation = 100ms;
// ServerToClientPackets.xml: C1:F1:00 GameServerEntered, twelve bytes.
constexpr std::array<BYTE, 12> EntryPacket{0xC1, 12, 0xF1, 0x00, 1, 0, 1, '1', '0', '4', '0', '0'};

class LoopbackPeer
{
public:
    LoopbackPeer()
    {
        WSADATA data{};
        REQUIRE(WSAStartup(MAKEWORD(2, 2), &data) == 0);
        m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(m_listener != INVALID_SOCKET);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        REQUIRE(bind(m_listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        REQUIRE(listen(m_listener, 1) == 0);
        int size = sizeof(address);
        REQUIRE(getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
        port = ntohs(address.sin_port);
    }

    ~LoopbackPeer()
    {
        if (m_peer != INVALID_SOCKET) closesocket(m_peer);
        if (m_listener != INVALID_SOCKET) closesocket(m_listener);
        WSACleanup();
    }

    void Accept()
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_listener, &readable);
        timeval wait{5, 0};
        REQUIRE(select(0, &readable, nullptr, nullptr, &wait) == 1);
        m_peer = accept(m_listener, nullptr, nullptr);
        REQUIRE(m_peer != INVALID_SOCKET);
    }

    void SendEntry()
    {
        REQUIRE(send(m_peer, reinterpret_cast<const char*>(EntryPacket.data()),
                     static_cast<int>(EntryPacket.size()), 0) == EntryPacket.size());
    }

    bool WaitClosed()
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(m_peer, &readable);
        timeval wait{5, 0};
        if (select(0, &readable, nullptr, nullptr, &wait) != 1) return false;
        char byte;
        const int count = recv(m_peer, &byte, 1, 0);
        return count == 0 || (count == SOCKET_ERROR && WSAGetLastError() == WSAECONNRESET);
    }

    int port = 0;

private:
    SOCKET m_listener = INVALID_SOCKET;
    SOCKET m_peer = INVALID_SOCKET;
};

struct CallbackGate;
std::atomic<CallbackGate*> activeGate{};

struct CallbackGate
{
    CallbackGate()
        : entered(enteredPromise.get_future()), completed(completedPromise.get_future()),
          released(releasePromise.get_future().share())
    {
        activeGate = this;
    }

    ~CallbackGate()
    {
        Release();
        activeGate = nullptr;
        delete owner.exchange(nullptr);
    }

    void Release()
    {
        if (!releaseSet.exchange(true)) releasePromise.set_value();
    }

    void Receive()
    {
        if (calls.fetch_add(1) != 0) return;
        enteredPromise.set_value();
        if (auto* connection = owner.exchange(nullptr))
            delete connection;
        else if (released.wait_for(CompletionDeadline) != std::future_status::ready)
            timedOut = true;
        completedPromise.set_value();
    }

    std::promise<void> enteredPromise, completedPromise, releasePromise;
    std::future<void> entered, completed;
    std::shared_future<void> released;
    std::atomic<bool> releaseSet{}, timedOut{};
    std::atomic<int> calls{};
    std::atomic<Connection*> owner{};
};

void ReceivePacket(int32_t, const BYTE*, int32_t)
{
    if (auto* gate = activeGate.load()) gate->Receive();
}

struct ReleaseOnExit
{
    CallbackGate& gate;
    ~ReleaseOnExit() { gate.Release(); }
};

std::unique_ptr<Connection> Connect(LoopbackPeer& peer)
{
    auto connection = std::make_unique<Connection>(u"127.0.0.1", peer.port, false, &ReceivePacket);
    REQUIRE(connection->IsConnected());
    peer.Accept();
    return connection;
}
} // namespace

TEST_CASE("Connection destruction waits for an in-flight native callback [network][connection-lifetime][inflight]")
{
    CallbackGate gate;
    LoopbackPeer peer;
    auto connection = Connect(peer);
    peer.SendEntry();
    REQUIRE(gate.entered.wait_for(CompletionDeadline) == std::future_status::ready);

    std::promise<void> destructionStarted;
    auto started = destructionStarted.get_future();
    auto destroying = std::async(std::launch::async,
        [owned = std::move(connection), &destructionStarted]() mutable {
            destructionStarted.set_value();
            owned.reset();
        });
    ReleaseOnExit release{gate}; // Unblocks the callback before the future joins on every exit.
    REQUIRE(started.wait_for(CompletionDeadline) == std::future_status::ready);
    CHECK(destroying.wait_for(HeldCallbackObservation) == std::future_status::timeout);
    gate.Release();
    REQUIRE(destroying.wait_for(CompletionDeadline) == std::future_status::ready);
    destroying.get();
    REQUIRE(gate.completed.wait_for(CompletionDeadline) == std::future_status::ready);
    CHECK_FALSE(gate.timedOut.load());
    CHECK(peer.WaitClosed());
}

TEST_CASE("A native packet callback can destroy its connection without deadlock [network][connection-lifetime][reentrant]")
{
    CallbackGate gate;
    LoopbackPeer peer;
    auto connection = Connect(peer);
    gate.owner = connection.release();
    peer.SendEntry();
    const auto completed = gate.completed.wait_for(CompletionDeadline);
    CHECK(completed == std::future_status::ready);
    delete gate.owner.exchange(nullptr);
    CHECK(gate.calls.load() == 1);
    CHECK(peer.WaitClosed());
}

TEST_CASE("Late native callbacks ignore a destroyed connection handle [network][connection-lifetime][late]")
{
    CallbackGate gate;
    LoopbackPeer firstPeer, replacementPeer;
    alignas(Connection) std::byte storage[sizeof(Connection)];
    const auto destroy = [](Connection* connection) { connection->~Connection(); };
    using Owner = std::unique_ptr<Connection, decltype(destroy)>;
    Owner first(new (storage) Connection(u"127.0.0.1", firstPeer.port, false, &ReceivePacket), destroy);
    REQUIRE(first->IsConnected());
    firstPeer.Accept();
    const auto firstHandle = first->GetHandle();
    first.reset();

    // A normal reconnect can reuse the allocation while old managed shutdown is still queued.
    Owner replacement(new (storage) Connection(u"127.0.0.1", replacementPeer.port, false, &ReceivePacket), destroy);
    REQUIRE(replacement->IsConnected());
    replacementPeer.Accept();
    const auto replacementHandle = replacement->GetHandle();
    REQUIRE(replacementHandle != firstHandle);
    CHECK(firstPeer.WaitClosed());
    gate.Release();
    replacementPeer.SendEntry();
    REQUIRE(gate.completed.wait_for(CompletionDeadline) == std::future_status::ready);
    CHECK(replacement->GetHandle() == replacementHandle);
    CHECK(replacement->IsConnected());
    CHECK(gate.calls.load() == 1);
    replacement.reset();
    CHECK(replacementPeer.WaitClosed());
}
