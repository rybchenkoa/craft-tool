#include <fstream>

#include "Config.h"

bool Config::read_from_file(const char *fileName)
{
    positions.clear();
    records.clear();

    std::ifstream file(fileName); //открываем файл

    if(!file)
        return false;

    while(file) //пока он не закончился
    {
        records.emplace_back(KeyData());
        KeyData &value = records.back();
        std::getline(file, value.record); //читаем строку
        size_t pos = 0;
        auto whitespace = [&pos, &value]()
        {
            while(pos < value.record.size() && (value.record[pos] == ' ' || value.record[pos] == '\t'))
                ++pos;
        };

        auto not_whitespace = [&pos, &value]()
        {
            while(pos < value.record.size() && value.record[pos] != ' ' && value.record[pos] != '\t')
                ++pos;
        };

        auto is_comment = [&pos, &value]()
        {
            return pos + 1 < value.record.size() && value.record[pos] == '/' && value.record[pos+1] == '/';
        };

        auto skip_string = [&pos, &value]()
        {
			++pos;
            while(pos < value.record.size() && value.record[pos++] != '\"')
                ;
        };

        whitespace();  //пропускаем пробелы в начале
        if(pos == value.record.size()) //пустая строка
            continue;
        if(is_comment())
            continue;
        size_t keyStart = pos;
        not_whitespace();  //пропускаем символы ключа
        std::string key = value.record.substr(keyStart, pos - keyStart);
        positions[key] = --records.end();

        whitespace(); //пробелы после ключа
        value.valueStart = pos;
        if(pos == value.record.size())
        {
            value.valueEnd = pos;
            continue;
        }

        bool isString = value.record[pos] == '\"';
        if(isString)
        {
            skip_string();
            value.valueEnd = pos;
        }
        else
        {
            size_t endPos = value.record.find("//", pos);
            if(endPos == std::string::npos)
                endPos = value.record.size();
            not_whitespace();
            value.valueEnd = std::min(pos, endPos);
        }
    }

    return true;
}

bool Config::save_to_file(const char *fileName)
{
    std::ofstream file(fileName); //открываем файл

    if(!file)
        return false;

    for(auto iter = records.begin(); iter != records.end(); ++iter)
        file << iter->record << std::endl;

    return true;
}

std::string Config::KeyData::get_value()
{
    if(record[valueStart] == '\"')
        return record.substr(valueStart + 1, valueEnd - valueStart - 2);
    else
        return record.substr(valueStart, valueEnd - valueStart);
}

bool Config::get_int(const char *key, int &value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
        return false;
    std::string text = iter->second->get_value();
    sscanf(text.c_str(), "%d", &value);

    return true;
}

bool Config::get_float(const char *key, float &value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
        return false;
    std::string text = iter->second->get_value();
    sscanf(text.c_str(), "%f", &value);

    return true;
}
bool Config::get_string(const char *key, std::string &value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
        return false;
    value = iter->second->get_value();

    return true;
}

int Config::get_int_def(const char *key, int def)
{
    auto iter = positions.find(key);
    if(iter != positions.end()) {
        std::string text = iter->second->get_value();
        sscanf(text.c_str(), "%d", &def);
    }

    return def;
}

void Config::set_int(const char *key, int value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
    {
        records.emplace_back(KeyData());
        KeyData &data = records.back();
        data.record = key;
        data.record += ' ';
        data.valueStart = data.record.size();
        data.record += value;
        data.valueEnd = data.record.size();
        return;
    }
    auto &data = *iter->second;
    auto prevRecord = data.record;
    data.record = prevRecord.substr(0, data.valueStart);
    data.record += value;
    data.record += prevRecord.substr(data.valueEnd);
}

void Config::set_float(const char *key, float value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
    {
        records.emplace_back(KeyData());
        KeyData &data = records.back();
        data.record = key;
        data.record += ' ';
        data.valueStart = data.record.size();
        data.record += std::to_string((long double)value);
        data.valueEnd = data.record.size();
        return;
    }
    auto &data = *iter->second;
    auto prevRecord = data.record;
    data.record = prevRecord.substr(0, data.valueStart);
    data.record += std::to_string((long double)value);
    data.record += prevRecord.substr(data.valueEnd);
}

void Config::set_string(const char *key, std::string &value)
{
    auto iter = positions.find(key);
    if(iter == positions.end())
    {
        records.emplace_back(KeyData());
        KeyData &data = records.back();
        data.record = key;
        data.record += ' ';
        data.valueStart = data.record.size();
        data.record += '\"';
        data.record += value;
        data.record += '\"';
        data.valueEnd = data.record.size();
        return;
    }
    auto &data = *iter->second;
    auto prevRecord = data.record;
    data.record = prevRecord.substr(0, data.valueStart);
    data.record += '\"';
    data.record += value;
    data.record += '\"';
    data.record += prevRecord.substr(data.valueEnd);
}
