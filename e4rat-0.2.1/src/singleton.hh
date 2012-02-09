/*
 * singleton.hh - Macros declare a class to be a heap based singleton
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
 * Exemplary usage:
 * 
 * class MyClass
 * {
 *     DECLARE_SINGLETON(MyClass);
 *     public:
 *        void foo();
 * };
 * 
 * DEFINE_SINGLETON(MyClass);
 */

#ifndef SINGLETON_HH
#define SINGLETON_HH

#include <pthread.h>

#define DECLARE_SINGLETON(ClassName)                            \
    public:                                                     \
      static ClassName* instance();                             \
private:                                                        \
    static ClassName *me;                                       \
    static pthread_mutex_t lock_singleton;                      \
    ClassName();                                                \
    ClassName( const ClassName& );                              \
    ~ClassName();                                               \
                                                                \
    /* Guarder is responsible to cleanup */                     \
    class Guarder {                                             \
        public: ~Guarder() {                                    \
            if( ClassName::me != 0 )                            \
                delete ClassName::me;                           \
        }                                                       \
    };                                                          \
    friend class Waechter;
    
#define DEFINE_SINGLETON(ClassName)                                     \
    ClassName* ClassName::me = NULL;                                    \
    pthread_mutex_t ClassName::lock_singleton = PTHREAD_MUTEX_INITIALIZER; \
    ClassName* ClassName::instance()                             \
    {                                                                   \
        static Guarder g;                                              \
        if( me == NULL )                                                \
            if(0 == pthread_mutex_lock(&lock_singleton)) {              \
                me = new ClassName();                                   \
                pthread_mutex_unlock(&lock_singleton);                  \
            }                                                           \
                                                                        \
        return me;                                                      \
    }
    

#endif
