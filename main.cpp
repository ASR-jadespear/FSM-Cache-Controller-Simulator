#include <iostream>
#include <vector>
#include <queue>
#include <iomanip>
#include <sstream>
#include <cstdint>

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
    int timer = 0;
    bool active = false;

public:
    Memory(int latency = 3) : latency(latency) {}

    void start()
    {
        active = true;
        timer = latency;
    }

    bool tick()
    {
        if (active)
        {
            timer--;
            if (timer <= 0)
            {
                active = false;
                return true;
            }
        }
        return false;
    }

    bool isBusy() const { return active; }
    int getTimer() const { return timer; }
};

class CacheSimulator
{
private:
    static const int CACHE_SIZE = 1024;
    static const int OFFSET_BITS = 4;
    static const int INDEX_BITS = 10;
    static const int TAG_BITS = 18;

    vector<CacheBlock> cache;
    State state = IDLE;

    Memory memory;

    queue<Request> cpuQueue;

    Request currentRequest;
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
        bool memReadyThisCycle = memory.tick();

        string currentStateStr;
        if (state == IDLE)
            currentStateStr = "IDLE";
        else if (state == COMPARE_TAG)
            currentStateStr = "COMP_TAG";
        else if (state == WRITE_BACK)
            currentStateStr = "WRITE_BACK";
        else if (state == ALLOCATE)
            currentStateStr = "ALLOCATE";

        string reqStr = "-";
        string hitMissStr = "-";
        string actionStr = "-";
        string conceptStr = "-";

        if (state != IDLE)
        {
            reqStr = (currentRequest.isWrite ? "Write " : "Read  ") + toHex(currentRequest.address);
        }

        switch (state)
        {
        case IDLE:
            if (!cpuQueue.empty())
            {
                currentRequest = cpuQueue.front();
                cpuQueue.pop();
                reqStr = (currentRequest.isWrite ? "Write " : "Read  ") + toHex(currentRequest.address);
                actionStr = "Loaded new CPU request";
                conceptStr = "Fetching next instruction/data";
                state = COMPARE_TAG;
                if (currentRequest.isWrite)
                    totalWrites++;
                else
                    totalReads++;
            }
            else
            {
                actionStr = "Waiting for CPU";
                conceptStr = "Stall cycles";
            }
            break;

        case COMPARE_TAG:
        {
            uint32_t offsetMask = (1U << OFFSET_BITS) - 1;
            uint32_t indexMask = (1U << INDEX_BITS) - 1;
            uint32_t tagMask = (1U << TAG_BITS) - 1;

            uint32_t offset = currentRequest.address & offsetMask;
            uint32_t index = (currentRequest.address >> OFFSET_BITS) & indexMask;
            uint32_t tag = (currentRequest.address >> (OFFSET_BITS + INDEX_BITS)) & tagMask;
            uint32_t wordIndex = offset / 4;

            if (cache[index].valid && cache[index].tag == tag)
            {
                hitMissStr = "HIT";
                totalHits++;
                if (currentRequest.isWrite)
                {
                    cache[index].dirty = true;
                    cache[index].data[wordIndex] = currentRequest.data;
                    actionStr = "Write word " + to_string(wordIndex) + ", set dirty=1";
                    conceptStr = "Write Hit (Delaying memory write)";
                }
                else
                {
                    stringstream ss;
                    ss << "Read word " << wordIndex << " (0x" << hex << cache[index].data[wordIndex] << dec << ")";
                    actionStr = ss.str();
                    conceptStr = "Read Hit (Spatial/Temporal Locality)";
                }
                state = IDLE;
            }
            else
            {
                hitMissStr = "MISS";
                totalMisses++;
                bool wasDirty = cache[index].dirty;
                cache[index].tag = tag;

                if (cache[index].valid && wasDirty)
                {
                    actionStr = "Dirty block eviction triggered";
                    conceptStr = "Capacity/Conflict Miss (Requires Write-Back)";
                    state = WRITE_BACK;
                    totalWritebacks++;
                }
                else
                {
                    actionStr = "Clean block allocation triggered";
                    conceptStr = "Compulsory/Clean Miss (Direct Allocate)";
                    state = ALLOCATE;
                    totalAllocations++;
                }
                memory.start();
            }
            break;
        }

        case WRITE_BACK:
            if (memReadyThisCycle)
            {
                actionStr = "Mem write complete -> Allocate";
                conceptStr = "Stale data successfully synced";
                state = ALLOCATE;
                totalAllocations++;
                memory.start();
            }
            else
            {
                actionStr = "Writing old block to Mem (" + to_string(memory.getTimer()) + " left)";
                conceptStr = "Paying Write-Back penalty";
            }
            break;

        case ALLOCATE:
            if (memReadyThisCycle)
            {
                uint32_t indexMask = (1U << INDEX_BITS) - 1;
                uint32_t index = (currentRequest.address >> OFFSET_BITS) & indexMask;

                cache[index].valid = true;
                cache[index].dirty = false;
                for (int i = 0; i < 4; i++)
                {
                    cache[index].data[i] = 0;
                }
                actionStr = "Mem read complete -> Cache updated";
                conceptStr = "New block loaded (Write-Allocate)";
                state = COMPARE_TAG;
            }
            else
            {
                actionStr = "Reading new block from Mem (" + to_string(memory.getTimer()) + " left)";
                conceptStr = "Paying Miss penalty";
            }
            break;
        }

        string nextStateStr;
        if (state == IDLE)
            nextStateStr = "IDLE";
        else if (state == COMPARE_TAG)
            nextStateStr = "COMP_TAG";
        else if (state == WRITE_BACK)
            nextStateStr = "WRITE_BACK";
        else if (state == ALLOCATE)
            nextStateStr = "ALLOCATE";

        cout << left << setw(6) << cycle << " | " << setw(10) << currentStateStr << " | " << setw(16) << reqStr << " | " << setw(8) << hitMissStr << " | " << setw(35) << actionStr << " | " << setw(45) << conceptStr << " | " << setw(10) << nextStateStr << "\n";
    }

public:
    CacheSimulator(int latency = 3) : cache(CACHE_SIZE), memory(latency) {}

    void addRequest(bool isWrite, uint32_t address, uint32_t data)
    {
        cpuQueue.push({isWrite, address, data});
    }

    void run()
    {
        cout << left << setw(6) << "Cycle" << " | " << setw(10) << "State" << " | " << setw(16) << "Request" << " | " << setw(8) << "Hit/Miss" << " | " << setw(35) << "Action" << " | " << setw(45) << "Concept" << " | " << setw(10) << "Next State" << "\n";
        cout << string(150, '-') << "\n";

        while (!cpuQueue.empty() || state != IDLE || memory.isBusy())
        {
            step();
        }

        printStatistics();
    }

    void printStatistics()
    {
        cout << "\n============================================================\n";
        cout << "   Overall Statistics\n";
        cout << "============================================================\n";
        cout << "  Reads       : " << totalReads << "\n";
        cout << "  Writes      : " << totalWrites << "\n";
        cout << "  Hits        : " << totalHits << "\n";
        cout << "  Misses      : " << totalMisses << "\n";
        cout << "  Writebacks  : " << totalWritebacks << "\n";
        cout << "  Allocations : " << totalAllocations << "\n";

        double hitRate = 0.0;
        if (totalHits + totalMisses > 0)
        {
            hitRate = (static_cast<double>(totalHits) / (totalHits + totalMisses)) * 100.0;
        }
        cout << "  Hit Rate    : " << fixed << setprecision(1) << hitRate << "%\n";
        cout << "  Total cycles: " << cycle << "\n";
        cout << "============================================================\n\n";
    }
};

int main()
{
    CacheSimulator simulator(3);
    /* ========================================================
       Simulating: for(int i=0; i<8; i++) { arr[i] *= 2; }
       ======================================================== */

    // --- Processing elements 0 through 3 (Fits in Block 1) ---
    simulator.addRequest(false, 0x0000A000, 0); // Read arr[0] -> Compulsory Miss (Fetches arr[0-3])
    simulator.addRequest(true, 0x0000A000, 10); // Write arr[0] -> Hit (Sets Dirty)

    simulator.addRequest(false, 0x0000A004, 0); // Read arr[1] -> HIT (Spatial Locality)
    simulator.addRequest(true, 0x0000A004, 20); // Write arr[1] -> HIT

    simulator.addRequest(false, 0x0000A008, 0); // Read arr[2] -> HIT
    simulator.addRequest(true, 0x0000A008, 30); // Write arr[2] -> HIT

    simulator.addRequest(false, 0x0000A00C, 0); // Read arr[3] -> HIT
    simulator.addRequest(true, 0x0000A00C, 40); // Write arr[3] -> HIT

    // --- Processing elements 4 through 7 (Fits in Block 2) ---
    simulator.addRequest(false, 0x0000A010, 0); // Read arr[4] -> Miss (Fetches arr[4-7])
    simulator.addRequest(true, 0x0000A010, 50); // Write arr[4] -> HIT (Sets Dirty)

    simulator.addRequest(false, 0x0000A014, 0); // Read arr[5] -> HIT
    simulator.addRequest(true, 0x0000A014, 60); // Write arr[5] -> HIT

    simulator.addRequest(false, 0x0000A018, 0); // Read arr[6] -> HIT
    simulator.addRequest(true, 0x0000A018, 70); // Write arr[6] -> HIT

    simulator.addRequest(false, 0x0000A01C, 0); // Read arr[7] -> HIT
    simulator.addRequest(true, 0x0000A01C, 80); // Write arr[7] -> HIT

    simulator.run();

    return 0;
}