#include <map>
#include <string>
#include <list>

class Config
{
public:
    bool read_from_file(const char *fileName);
    bool save_to_file(const char *fileName);

    bool get_int(const char *key, int &value);
    bool get_float(const char *key, float &value);
    bool get_string(const char *key, std::string &value);

    int get_int_def(const char *key, int def);

    void set_int(const char *key, int value);
    void set_float(const char *key, float value);
    void set_string(const char *key, std::string &value);

private:
    struct KeyData
    {
        int valueStart;    //первый символ значения
        int valueEnd;      //последний символ значения
        std::string record;//прочитанная из файла строка
        KeyData():valueStart(0), valueEnd(0){}
        std::string get_value();
    };

    std::list<KeyData> records; //все строки файла
    std::map<std::string, std::list<KeyData>::iterator> positions;
};
