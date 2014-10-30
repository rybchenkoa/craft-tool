//приём пакетов устрройством от порта
class IPortToDevice
{
public:
    virtual bool on_packet_received(char *data, int size)=0;
};
