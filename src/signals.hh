/*
 * signals.hh - Macros allow easy signal/slot handling
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
 *
 *
 * Following macros are based on boost::signals2 which is thread-safe
 *
 * Declare a signal witch SIGNAL() only in c++ class statement.
 * Use CONNECT() and DISCONNECT() to link or unlink a signal with a specific slot
 *
 * Exemplary usage:
 *
 * Class Foo {
 *         SIGNAL(mysig, int);
 *     public:
 * }
 * Class Bar {
 *     public:
 *         void mylot(int);
 * };
 * void main()
 * {
 *     Foo foo;
 *     Bar bar;
 *     CONNECT(&foo, mysig, boost::bind(&Bar::myslot, &bar, _1));
 * }
 */
#include <boost/signals2/signal.hpp>

//get the number of arguments
#define BIND1ST _1
#define BIND2ST _1,_2
#define BIND3ST _1,_2,_3
#define BIND4ST _1,_2,_3,_4
#define BIND5ST _1,_2,_3,_4,_5

#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__,                 \
                                          BIND5ST,                     \
                                          BIND4ST,                     \
                                          BIND3ST,                     \
                                          BIND2ST,                     \
                                          BIND1ST)
#define VA_NUM_ARGS_IMPL(_1,_2,_3,_4,_5,N,...)N

#define DECLARE_SIGNAL(signame, args...)                                   \
    boost::signals2::signal<void ( args )> signame

#define DECLARE_CONNECT_FUNCTION(signame, args...)                         \
    boost::signals2::connection doOn##signame(                             \
       const boost::signals2::signal<void ( args )>::slot_type &subscriber)\
    {                                                                      \
        return signame.connect(subscriber);                                \
    }


#define DECLARE_DISCONNECT_FUNCTION(signame, args...)                       \
    template <typename T>                                                   \
    void undoOn##signame(void (T::*func)(args), T* obj)                     \
    {                                                                       \
        if(sizeof(#args) == sizeof(""))                                     \
            signame.disconnect(boost::bind(func, obj));                     \
        else                                                                \
            signame.disconnect(boost::bind(func, obj, VA_NUM_ARGS(args)));  \
    }


#define SIGNAL(name, args...)                                               \
    public:                                                                 \
    DECLARE_CONNECT_FUNCTION(name,args)                                     \
    DECLARE_DISCONNECT_FUNCTION(name,args)                                  \
    protected:                                                              \
    DECLARE_SIGNAL(name,args)



#define CONNECT(sender, signal, slot) (sender)->doOn##signal(slot)
#define DISCONNECT(sender, signal, slot, me) (sender)->undoOn##signal(slot, me)


