#pragma once

class Config
{
public:
    bool read_from_file(const char *fileName);
    bool save_to_file(const char *fileName);

    bool get_int(const char *key, int &value);
    bool get_float(const char *key, float &value);
	bool get_string(const char *key, std::string &value);
	bool get_array(const char *key, std::vector<std::string> &value);

    int get_int_def(const char *key, int def);

    void set_int(const char *key, int value);
    void set_float(const char *key, float value);
    void set_string(const char *key, std::string &value);

private:
    struct KeyData
    {
        int valueStart = 0;  // первый символ значения
        int valueEnd = 0;    // последний символ значения
        std::string record;  // прочитанная из файла строка
        std::string get_value();
    };

    std::list<KeyData> records; //все строки файла
    std::map<std::string, std::list<KeyData>::iterator> positions;
};

extern Config *g_config;

std::vector<std::string> split_string(std::string& value, char delimiter);
