/*
 * config.hh - global settings and config file parser
 *
 * Copyright (C) 2011 by Andreas Rid
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_HH
#define CONFIG_HH

#include "singleton.hh"
#include <boost/property_tree/ptree.hpp>

/*
 * Default parameters are stored in defaultProperty Tree.
 * Runtime changes or parsed options parsed from config file
 * are stored in ptree.
 *
 * First look into ptree whether options exists. If not use
 * default Value from defaultProperty.
 *
 * Config throws an exception when no property got found.
 * Therefore don't forget to insert an default property
 * in Config::Config()
 */
class Config
{
        DECLARE_SINGLETON(Config);
    public:
        template <typename T>
        static void set(std::string, T);
        template <typename T>
        static T get(std::string opt);

        void load();
        void dump();
        void clear();
        template <typename T>
        void _set(std::string, T);
        template <typename T>
        T _get(std::string opt);
    private:
        std::string defaultSection;
        boost::property_tree::ptree ptree;
        boost::property_tree::ptree defaultProperty;
};


template<typename T>
T Config::get(std::string opt)
{
    return instance()->_get<T>(opt);
}
template<typename T>
void Config::set(std::string opt, T val)
{
    instance()->_set(opt, val);
}

template<typename T>
void Config::_set(std::string opt, T val)
{
    std::stringstream ss;
    std::string sv;
    ss << val;
    ss >> sv;
    ptree.put(opt, sv);
}

template<typename T>
T Config::_get(std::string opt)
{
    boost::optional<T> value;

    if(defaultSection.size())
    {
        value = ptree.get_optional<T>(defaultSection + "." + opt);
        if(value)
            return *value;
    }
    
    value = ptree.get_optional<T>(opt);
    if(value)
        return *value;
    
    value =  defaultProperty.get_optional<T>(opt);
    if(value)
        return *value;

    throw std::invalid_argument(opt + ": unknown option");
}

#endif
