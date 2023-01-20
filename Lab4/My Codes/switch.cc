#include <cstdio>
#include <cstring>
#include "switch.h"
#include "types.h"

struct switchTableEntry
{
    int used{};
    mac_addr_t macAdd{};
    int port{};
    int lifeTime{};
};

class mySwitch : public SwitchBase
{
public:
    int PortNum{};
    switchTableEntry* switchTable{};
    void InitSwitch(int numPorts) override;
    int ProcessFrame(int inPort, char* framePtr) override;
};



void mySwitch::InitSwitch(int numPorts)
{
    PortNum = numPorts;
    switchTable = new switchTableEntry[PortNum + 1000];
    for(int i = 0; i < PortNum + 1000; i++)
    {
        switchTable[i].used = 0;
        switchTable[i].port = -1;
        memset(switchTable[i].macAdd,0,sizeof(mac_addr_t));
        switchTable[i].lifeTime = 0;
    }
}

int mySwitch::ProcessFrame(int inPort, char *framePtr)
{
    if(inPort == 1) // control frame
    {
        auto* header = (ether_header_t*)framePtr;
        if(header->ether_type != ETHER_CONTROL_TYPE)
        {
            //error
            printf("Received a frame from Port 1 which is not a control frame!\n");
            return -1;
        }
        for(int i = 0; i < PortNum + 1000; i++)
        {
            if(switchTable[i].used == 1)
            {
                switchTable[i].lifeTime--;
            }
            if(switchTable[i].lifeTime <= 0)
                switchTable[i].used = 0;
        }
        return -1;
    }

    else
    {
        auto* header = (ether_header_t*)framePtr;
        if(header->ether_type != ETHER_DATA_TYPE)
        {
            printf("Received a frame from a normal port which is not a data frame!\n");
            return -1;
        }
        int upgrade = 0;
        for(int i = 0; i < PortNum + 1000; i++)
        {
            if(switchTable[i].used == 1 &&
                memcmp(switchTable[i].macAdd,header->ether_src,sizeof(mac_addr_t)) == 0)
            {
                switchTable[i].lifeTime = 10;
                switchTable[i].port = inPort;
                upgrade = 1;
            }
        }
        if(!upgrade) // insert a new entry
        {
            for(int i = 0; i < PortNum + 1000; i++)
            {
                if(switchTable[i].used == 0)
                {
                    switchTable[i].used = 1;
                    switchTable[i].lifeTime = 10;
                    switchTable[i].port = inPort;
                    memcpy(switchTable[i].macAdd,header->ether_src,sizeof(mac_addr_t));
                    break;
                }
            }
        }
        int outPort = 0;
        for(int i = 0; i < PortNum + 1000; i++)
        {
            if(switchTable[i].used == 1 &&
                memcmp(switchTable[i].macAdd,header->ether_dest,sizeof(mac_addr_t)) == 0)
            {
                //find an entry
                outPort = switchTable[i].port;
                break;
            }
        }
        if(outPort == 0)
            return 0;
        else if(outPort == inPort)
            return -1;
        else
            return outPort;
    }

}



SwitchBase* CreateSwitchObject() {
  return new mySwitch();
  //return nullptr;
}
