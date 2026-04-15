#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <unordered_set>

using namespace std;

enum State
{
    IDLE,
    COMPARE_TAG,
    WRITE_BACK,
    ALLOCATE
};

struct Request
{
    bool isWrite;
    uint32_t address;
    uint32_t data;
};

struct CacheBlock
{
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    uint32_t data[4] = {0};
};

class Memory
{
private:
    int latency;
    int remainingCycles = 0;
    bool active = false;

public:
    Memory(int latency = 3) : latency(latency) {}

    void start()
    {
        active = true;
        remainingCycles = latency;
    }

    bool tick()
    {
        if (active)
        {
            remainingCycles--;
            if (remainingCycles <= 0)
            {
                active = false;
                return true;
            }
        }
        return false;
    }

    bool isBusy() const { return active; }
    int getTimer() const { return remainingCycles; }
};

class CacheSimulator
{
private:
    static const int CACHE_SIZE = 1024;
    static const int OFFSET_BITS = 4;
    static const int INDEX_BITS = 10;

    vector<CacheBlock> cache;
    unordered_set<uint32_t> accessedBlockAddresses;

    State state = IDLE;
    Memory memory;
    queue<Request> pendingCpuRequests;

    Request currentRequest;
    bool isRetryingMiss = false;
    int cycle = 0;

    int totalReads = 0;
    int totalWrites = 0;
    int totalHits = 0;
    int totalMisses = 0;
    int totalWritebacks = 0;
    int totalAllocations = 0;

    string toHex(uint32_t addr)
    {
        stringstream ss;
        ss << "0x" << hex << uppercase << setfill('0') << setw(8) << addr;
        return ss.str();
    }

    void step()
    {
        cycle++;
        bool isMemoryReady = memory.tick();

        string stateStr = (state == IDLE) ? "IDLE" : (state == COMPARE_TAG) ? "COMP_TAG"
                                                 : (state == WRITE_BACK)    ? "WRITE_BACK"
                                                                            : "ALLOCATE";

        string reqStr = "-";
        string hitMissStr = "-";
        string actionStr = "-";
        string conceptStr = "-";

        if (state != IDLE)
            reqStr = (currentRequest.isWrite ? "Write " : "Read  ") + toHex(currentRequest.address);

        uint32_t offsetMask = (1U << OFFSET_BITS) - 1;
        uint32_t indexMask = (1U << INDEX_BITS) - 1;

        uint32_t offset = currentRequest.address & offsetMask;
        uint32_t index = (currentRequest.address >> OFFSET_BITS) & indexMask;
        uint32_t tag = currentRequest.address >> (OFFSET_BITS + INDEX_BITS);
        uint32_t wordIndex = offset / 4;

        switch (state)
        {
        case IDLE:
            if (!pendingCpuRequests.empty())
            {
                currentRequest = pendingCpuRequests.front();
                pendingCpuRequests.pop();
                isRetryingMiss = false;

                reqStr = (currentRequest.isWrite ? "Write " : "Read  ") + toHex(currentRequest.address);
                actionStr = "Loaded CPU request";
                conceptStr = "Fetch next op";

                if (currentRequest.isWrite)
                    totalWrites++;
                else
                    totalReads++;

                state = COMPARE_TAG;
            }
            else
            {
                actionStr = "Idle";
            }
            break;

        case COMPARE_TAG:
        {
            bool hit = cache[index].valid && cache[index].tag == tag;

            if (hit)
            {
                if (!isRetryingMiss)
                {
                    hitMissStr = "HIT";
                    totalHits++;
                }
                else
                {
                    hitMissStr = "MISS";
                }

                if (currentRequest.isWrite)
                {
                    cache[index].dirty = true;
                    cache[index].data[wordIndex] = currentRequest.data;
                    actionStr = "Write word " + to_string(wordIndex);
                    conceptStr = "Write Hit";
                }
                else
                {
                    stringstream ss;
                    ss << "Read word " << wordIndex << " = " << cache[index].data[wordIndex];
                    actionStr = ss.str();
                    conceptStr = "Read Hit";
                }

                state = IDLE;
            }
            else
            {
                hitMissStr = "MISS";
                totalMisses++;
                isRetryingMiss = true;

                bool wasDirty = cache[index].dirty;
                uint32_t blockAddr = currentRequest.address >> OFFSET_BITS;
                bool firstTime = accessedBlockAddresses.find(blockAddr) == accessedBlockAddresses.end();

                if (cache[index].valid && wasDirty)
                {
                    actionStr = "Dirty eviction";
                    conceptStr = "Conflict Miss (Write-Back)";
                    state = WRITE_BACK;
                    totalWritebacks++;
                }
                else
                {
                    actionStr = "Allocate block";
                    conceptStr = firstTime ? "Compulsory Miss" : "Conflict Miss";
                    state = ALLOCATE;
                    totalAllocations++;
                }

                accessedBlockAddresses.insert(blockAddr);
                memory.start();
            }
            break;
        }

        case WRITE_BACK:
            if (isMemoryReady)
            {
                cache[index].dirty = false;
                actionStr = "Write-back done";
                conceptStr = "Memory synced";
                state = ALLOCATE;
                totalAllocations++;
                memory.start();
            }
            else
            {
                actionStr = "Writing (" + to_string(memory.getTimer()) + ")";
                conceptStr = "Write-back delay";
            }
            break;

        case ALLOCATE:
            if (isMemoryReady)
            {
                cache[index].valid = true;
                cache[index].dirty = false;
                cache[index].tag = tag;

                for (int i = 0; i < 4; i++)
                    cache[index].data[i] = (currentRequest.address & ~0xF) + i * 4;

                actionStr = "Block loaded";
                conceptStr = "Allocate";
                state = COMPARE_TAG;
            }
            else
            {
                actionStr = "Reading (" + to_string(memory.getTimer()) + ")";
                conceptStr = "Miss delay";
            }
            break;
        }

        string nextStateStr = (state == IDLE) ? "IDLE" : (state == COMPARE_TAG) ? "COMP_TAG"
                                                     : (state == WRITE_BACK)    ? "WRITE_BACK"
                                                                                : "ALLOCATE";

        cout << left << setw(6) << cycle << " | "
             << setw(10) << stateStr << " | "
             << setw(16) << reqStr << " | "
             << setw(12) << hitMissStr << " | "
             << setw(30) << actionStr << " | "
             << setw(30) << conceptStr << " | "
             << setw(10) << nextStateStr << "\n";
    }

public:
    CacheSimulator(int latency = 3)
        : cache(CACHE_SIZE), memory(latency) {}

    void addRequest(bool isWrite, uint32_t address, uint32_t data = 0)
    {
        pendingCpuRequests.push({isWrite, address, data});
    }

    void run()
    {
        cout << left << setw(6) << "Cycle" << " | "
             << setw(10) << "State" << " | "
             << setw(16) << "Request" << " | "
             << setw(12) << "Hit/Miss" << " | "
             << setw(30) << "Action" << " | "
             << setw(30) << "Concept" << " | "
             << setw(10) << "Next" << "\n";

        cout << string(130, '-') << "\n";

        while (!pendingCpuRequests.empty() || state != IDLE || memory.isBusy())
            step();

        printStats();
    }

    void printStats()
    {
        cout << "\n=== Stats ===\n";
        cout << "Reads: " << totalReads << "\n";
        cout << "Writes: " << totalWrites << "\n";
        cout << "Hits: " << totalHits << "\n";
        cout << "Misses: " << totalMisses << "\n";
        cout << "Writebacks: " << totalWritebacks << "\n";
        cout << "Allocations: " << totalAllocations << "\n";

        double hitRate = (totalHits + totalMisses)
                             ? (double)totalHits / (totalHits + totalMisses) * 100.0
                             : 0.0;

        cout << "Hit Rate: " << fixed << setprecision(1) << hitRate << "%\n";
        cout << "Cycles: " << cycle << "\n";
    }
};

int main()
{
    CacheSimulator sim(3);
    /*
        int* my_array = (int*)0x1000, var_A = (int*)0xA000, var_B = (int*)0xE000, var_C = (int*)0x5000;

        for (int i = 0; i < 4; i++) {
            int temp = my_array[i];       // CPU READS from 0x1000 + (i * 4)
            my_array[i] = (i + 1) * 10;   // CPU WRITES 10, 20, 30, 40 to 0x1000 + (i * 4)
        }

        int x = *var_A;                   // CPU READS from 0xA000
        int y = *var_B;                   // CPU READS from 0xE000
        *var_C = 15;                      // CPU WRITES 15 to 0x5000
    */
    sim.addRequest(false, 0x1000);
    sim.addRequest(true, 0x1000, 10);
    sim.addRequest(false, 0x1004);
    sim.addRequest(true, 0x1004, 20);
    sim.addRequest(false, 0x1008);
    sim.addRequest(true, 0x1008, 30);
    sim.addRequest(false, 0x100C);
    sim.addRequest(true, 0x100C, 40);

    sim.addRequest(false, 0xA000);
    sim.addRequest(false, 0xE000);

    sim.addRequest(true, 0x5000, 15);

    sim.run();

    return 0;
}